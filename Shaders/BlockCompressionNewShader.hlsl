#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

// shader implementation of new block compression (e.g. BC6H) in UHE
#define BLOCK_SIZE 4

// 128-bit output
struct UHBlockCompressionOutput
{
    uint LowBits0;
    uint LowBits1;
    uint HighBits0;
    uint HighBits1;
};

struct UHBC6HData
{
    uint LowBits;
    uint HighBits;
    uint3 Color0;
    uint3 Color1;
};

RWStructuredBuffer<UHBlockCompressionOutput> GResult : register(u0);
// input should be int, note this already stores a direct binary value of 16-bit float
StructuredBuffer<uint3> GColorInput : register(t1);
cbuffer BlockCompressionConstant : register(b2)
{
    uint GWidth;
    uint GHeight;
}

groupshared uint3 GBlockColors[16];
groupshared float GMinError[16];
groupshared UHBC6HData GMinResult[16];

void GetBC6HPalette(int3 Color0, int3 Color1, inout float3 OutPalette[16])
{
    int Weights[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

	// interpolate 16 values for comparison
    for (int Idx = 0; Idx < 16; Idx++)
    {
        int3 Palette = (Color0 * (64 - Weights[Idx]) + Color1 * Weights[Idx] + 32) >> 6;
        OutPalette[Idx] = Palette;
    }
}

// BC6H references:
// https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc6h-format
// Color0 and Color1 might be swapped during the process
UHBC6HData EvaluateBC6H(uint3 Color0, uint3 Color1, out float OutMinDiff)
{
	// setup palette for BC6H
    float3 Palette[16];
    GetBC6HPalette(Color0, Color1, Palette);

    OutMinDiff = 0;
    int BitShiftStart = 0;
    UHBC6HData Data = (UHBC6HData) 0;
    
    for (uint Idx = 0; Idx < 16; Idx++)
    {
        float3 BlockColor = GBlockColors[Idx];
        float MinDiff = UH_FLOAT_MAX;
        uint ClosestIdx = 0;

        for (uint Jdx = 0; Jdx < 16; Jdx++)
        {
            float Diff = length(BlockColor - Palette[Jdx]);
            if (Diff < MinDiff)
            {
                MinDiff = Diff;
                ClosestIdx = Jdx;
            }
        }

		// since the MSB of the first index will be discarded, if closest index for the first is larger than 3-bit range
		// swap the reference color and search again
        if (ClosestIdx > 7 && Idx == 0)
        {
            uint3 Temp = Color0;
            Color0 = Color1;
            Color1 = Temp;
            GetBC6HPalette(Color0, Color1, Palette);
            ClosestIdx = 16 - ClosestIdx - 1;
        }

        OutMinDiff += MinDiff;
        if (Idx < 8)
        {
            Data.LowBits |= ClosestIdx << BitShiftStart;
        }
        else if (Idx == 8)
        {
            // across the boundary of low bits and high bits
            Data.LowBits |= ClosestIdx << BitShiftStart;
            Data.HighBits = ClosestIdx >> 1;
            BitShiftStart = 3;
        }
        else
        {
            Data.HighBits |= ClosestIdx << BitShiftStart;
        }
        
        if (Idx != 8)
        {
            BitShiftStart += (Idx == 0) ? 3 : 4;
        }
    }

    Data.Color0 = Color0;
    Data.Color1 = Color1;
    return Data;
}

// all quantize below is assumed as signed
int QuantizeAsNBit(int InVal, int InBit)
{
    int S = 0;
    if (InVal < 0)
    {
        S = 1;
        InVal = -InVal;
    }

    int Q = (InVal << (InBit - 1)) / (0x7bff + 1);
    if (S)
    {
        Q = -Q;
    }

    return Q;
}

int UnquantizeFromNBit(int InVal, int InBit)
{
    int S = 0;
    if (InVal < 0)
    {
        S = 1;
        InVal = -InVal;
    }

    int Q;
    if (InVal == 0)
    {
        Q = 0;
    }
    else if (InVal >= ((1U << (InBit - 1)) - 1))
    {
        Q = 0x7FFF;
    }
    else
    {
        Q = ((InVal << 15) + 0x4000) >> (InBit - 1);
    }

    if (S)
    {
        Q = -Q;
    }

    return Q;
}

bool StoreBC6HMode14(UHBC6HData Data, out UHBlockCompressionOutput OutResult)
{
	// the first endpoint and the delta
	// 16 bits for the endpoint and 4 bits for the delta
    int RW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.r, 16), 16);
    int GW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.g, 16), 16);
    int BW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.b, 16), 16);
     
    int RX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.r - Data.Color0.r, 16), 16);
    int GX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.g - Data.Color0.g, 16), 16);
    int BX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.b - Data.Color0.b, 16), 16);
    if (!(RX >= -8 && RX < 7
			&& GX >= -8 && GX < 7
			&& BX >= -8 && BX < 7))
    {
        return false;
    }

	// store result from LSB to MSB
    int BitShiftStart = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;

	// first 5 bits for mode, mode 14 is 01111
    Result.LowBits0 = 15;
    BitShiftStart += 5;

	// store RW/GW/BW 10-bit [9:0]
    Result.LowBits0 |= (RW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits0 |= (GW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    // across boundary!
    Result.LowBits0 |= (BW & 127) << BitShiftStart;
    BitShiftStart += 7;
    Result.LowBits1 |= (BW >> 7) & 7;
    BitShiftStart = 3;

	// store RX[3:0]
    Result.LowBits1 |= (RX & 15) << BitShiftStart;
    BitShiftStart += 4;

	// store RW[10:15], note this is reversed!
    for (int Idx = 15; Idx >= 10; Idx--)
    {
        Result.LowBits1 |= (RW >> Idx & 1) << BitShiftStart;
        BitShiftStart++;
    }

	// store GX[3:0]
    Result.LowBits1 |= (GX & 15) << BitShiftStart;
    BitShiftStart += 4;

	// store GW[10:15], note this is reversed!
    for (Idx = 15; Idx >= 10; Idx--)
    {
        Result.LowBits1 |= (GW >> Idx & 1) << BitShiftStart;
        BitShiftStart++;
    }

	// store BX[3:0]
    Result.LowBits1 |= (BX & 15) << BitShiftStart;
    BitShiftStart += 4;

	// store BW[11:15], note this is reversed and across the low-high bits!
    for (Idx = 15; Idx >= 11; Idx--)
    {
        Result.LowBits1 |= (BW >> Idx & 1) << BitShiftStart;
        BitShiftStart++;
    }

    Result.HighBits0 |= (BW >> 10 & 1);
    BitShiftStart = 1;

    // now stores the indices, and acrossing boundary!
    Result.HighBits0 |= Data.LowBits << BitShiftStart;
    Result.HighBits1 |= (Data.LowBits >> 31);
    Result.HighBits1 |= Data.HighBits << 1;

    OutResult = Result;
    return true;
}

bool StoreBC6HMode13(UHBC6HData Data, out UHBlockCompressionOutput OutResult)
{
	// the first endpoint and the delta
	// 12 bits for the endpoint and 8 bits for the delta
    uint RW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.r, 12), 12) >> 4;
    uint GW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.g, 12), 12) >> 4;
    uint BW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.b, 12), 12) >> 4;

    int RX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.r - Data.Color0.r, 12), 12) >> 4;
    int GX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.g - Data.Color0.g, 12), 12) >> 4;
    int BX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.b - Data.Color0.b, 12), 12) >> 4;
    if (!(RX >= -128 && RX < 127
			&& GX >= -128 && GX < 127
			&& BX >= -128 && BX < 127))
    {
        return false;
    }

	// store result from LSB to MSB
    int BitShiftStart = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;

	// first 5 bits for mode, mode 13 is 01011
    Result.LowBits0 = 11;
    BitShiftStart += 5;

	// store RW/GW/BW 10-bit [9:0]
    Result.LowBits0 |= (RW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits0 |= (GW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits0 |= (BW & 127) << BitShiftStart;
    BitShiftStart += 7;
    Result.LowBits1 |= (BW >> 7) & 7;
    BitShiftStart = 3;

	// store RX[7:0]
    Result.LowBits1 |= (RX & 255) << BitShiftStart;
    BitShiftStart += 8;

	// store RW[10:11], note this is reversed!
    Result.LowBits1 |= (RW >> 11 & 1) << BitShiftStart;
    BitShiftStart++;
    Result.LowBits1 |= (RW >> 10 & 1) << BitShiftStart;
    BitShiftStart++;

	// store GX[7:0]
    Result.LowBits1 |= (GX & 255) << BitShiftStart;
    BitShiftStart += 8;

	// store GW[10:11], note this is reversed!
    Result.LowBits1 |= (GW >> 11 & 1) << BitShiftStart;
    BitShiftStart++;
    Result.LowBits1 |= (GW >> 10 & 1) << BitShiftStart;
    BitShiftStart++;

	// store BX[7:0]
    Result.LowBits1 |= (BX & 255) << BitShiftStart;
    BitShiftStart += 8;

	// store BW[10:11], note this is reversed and across the low-high bits!
    Result.LowBits1 |= (BW >> 11 & 1) << BitShiftStart;
    Result.HighBits0 |= (BW >> 10 & 1);
    BitShiftStart = 1;

    // now stores the indices, and acrossing boundary!
    Result.HighBits0 |= Data.LowBits << BitShiftStart;
    Result.HighBits1 |= (Data.LowBits >> 31);
    Result.HighBits1 |= Data.HighBits << 1;

    OutResult = Result;
    return true;
}

bool StoreBC6HMode12(UHBC6HData Data, out UHBlockCompressionOutput OutResult)
{
	// the first endpoint and the delta
	// 11 bits for the endpoint and 9 bits for the delta
	// for the odd bits, round up the value before shift
    uint RW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.r, 11), 11 + 32) >> 5;
    uint GW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.g, 11), 11 + 32) >> 5;
    uint BW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.b, 11), 11 + 32) >> 5;

    int RX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.r - Data.Color0.r, 11), 11 + 32) >> 5;
    int GX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.g - Data.Color0.g, 11), 11 + 32) >> 5;
    int BX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.b - Data.Color0.b, 11), 11 + 32) >> 5;
    if (!(RX >= -256 && RX < 255
			&& GX >= -256 && GX < 255
			&& BX >= -256 && BX < 255))
    {
        return false;
    }

	// store result from LSB to MSB
    int BitShiftStart = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;

	// first 5 bits for mode, mode 12 is 00111
    Result.LowBits0 = 7;
    BitShiftStart += 5;

	// store RW/GW/BW 10-bit [9:0]
    Result.LowBits0 |= (RW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits0 |= (GW & 1023) << BitShiftStart;
    BitShiftStart += 10;

    // across boundary!
    Result.LowBits0 |= (BW & 127) << BitShiftStart;
    BitShiftStart += 7;
    Result.LowBits1 |= (BW >> 7) & 7;
    BitShiftStart = 3;

	// store RX[8:0]
    Result.LowBits1 |= (RX & 511) << BitShiftStart;
    BitShiftStart += 9;

	// store RW[10]
    Result.LowBits1 |= (RW & 1024) << BitShiftStart;
    BitShiftStart++;

	// store GX[8:0]
    Result.LowBits1 |= (GX & 511) << BitShiftStart;
    BitShiftStart += 9;

	// store GW[10]
    Result.LowBits1 |= (GW & 1024) << BitShiftStart;
    BitShiftStart++;

	// store BX[8:0]
    Result.LowBits1 |= (BX & 511) << BitShiftStart;
    BitShiftStart += 9;

	// store BW[10]
    Result.HighBits0 |= (BW & 1024);
    BitShiftStart++;

    // now stores the indices, and acrossing boundary!
    Result.HighBits0 |= Data.LowBits << BitShiftStart;
    Result.HighBits1 |= (Data.LowBits >> 31);
    Result.HighBits1 |= Data.HighBits << 1;

    OutResult = Result;
    return true;
}

UHBlockCompressionOutput StoreBC6HMode11(UHBC6HData Data)
{
	// store result from LSB to MSB
    int BitShiftStart = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;

	// quantize and unquantize back for the consistency
    uint RW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.r, 10), 10) >> 6;
    uint GW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.g, 10), 10) >> 6;
    uint BW = UnquantizeFromNBit(QuantizeAsNBit(Data.Color0.b, 10), 10) >> 6;
    
    uint RX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.r, 10), 10) >> 6;
    uint GX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.g, 10), 10) >> 6;
    uint BX = UnquantizeFromNBit(QuantizeAsNBit(Data.Color1.b, 10), 10) >> 6;

	// first 5 bits for mode, mode 11 is corresponding to 00011
    Result.LowBits0 = 3;
    BitShiftStart += 5;

	// next 60 bits for reference colors, 10 bits for each color component
    Result.LowBits0 |= RW << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits0 |= GW << BitShiftStart;
    BitShiftStart += 10;

    // acrossing boundary!
    Result.LowBits0 |= BW << BitShiftStart;
    BitShiftStart += 7;
    Result.LowBits1 |= (BW >> 7);
    BitShiftStart = 3;

    Result.LowBits1 |= RX << BitShiftStart;
    BitShiftStart += 10;

    Result.LowBits1 |= GX << BitShiftStart;
    BitShiftStart += 10;

	// acrossing boundary!
    Result.LowBits1 |= BX << BitShiftStart;
    BitShiftStart += 9;

    Result.HighBits0 = BX >> 9;
    BitShiftStart = 1;

    // now stores the indices, and acrossing boundary!
    Result.HighBits0 |= Data.LowBits << BitShiftStart;
    Result.HighBits1 |= (Data.LowBits >> 31);
    Result.HighBits1 |= Data.HighBits << 1;

    return Result;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BlockCompressHDR(uint3 DTid : SV_DispatchThreadID, uint GIndex : SV_GroupIndex, uint3 Gid : SV_GroupID)
{
    // store block colors and init caches
    uint TextureIndex = DTid.x + DTid.y * GWidth;
    GBlockColors[GIndex] = GColorInput[TextureIndex];
    GMinError[GIndex] = UH_FLOAT_MAX;
    GroupMemoryBarrierWithGroupSync();
    
    if (DTid.x >= GWidth || DTid.y >= GHeight)
    {
        return;
    }
    
    // find min/max color
    uint3 MaxColor = 0;
    uint3 MinColor = 65504;
    for (uint Idx = 0; Idx < 16; Idx++)
    {
        MaxColor = max(MaxColor, GBlockColors[Idx]);
        MinColor = min(MinColor, GBlockColors[Idx]);
    }
    
    // Test the pairs that has the minimal errors except itself
    float BC6HMinError = UH_FLOAT_MAX;
    float MinError = 0;
    UHBC6HData Result = (UHBC6HData) 0;
    UHBC6HData FinalResult = (UHBC6HData) 0;
    
    // on CPU side a thread runs 120 tests
    // here, only need to run 15 tests for each thread, I'll choose the minimal result across all threads later
    for (Idx = 0; Idx < 16; Idx++)
    {
        if (Idx == GIndex)
        {
            continue;
        }
        
        // call EvaluateBC6H
        Result = EvaluateBC6H(GBlockColors[GIndex], GBlockColors[Idx], MinError);
        if (MinError < BC6HMinError)
        {
            BC6HMinError = MinError;
            FinalResult = Result;
        }
    }
  
    // in case the search doesn't work well, try the min/max color once
    Result = EvaluateBC6H(MaxColor, MinColor, MinError);
    if (MinError < BC6HMinError)
    {
        BC6HMinError = MinError;
        FinalResult = Result;
    }
    
    GMinError[GIndex] = BC6HMinError;
    GMinResult[GIndex] = FinalResult;
    GroupMemoryBarrierWithGroupSync();
    
    // each thread hold a minimal result now, find the real minimal one for this block and output
    if (GIndex == 0)
    {
        int MinIdx = 0;
        float MinError = UH_FLOAT_MAX;
        for (int Idx = 0; Idx < 16; Idx++)
        {
            if (GMinError[Idx] < MinError)
            {
                MinIdx = Idx;
                MinError = GMinError[Idx];
            }
        }
        
        // select the data output mode
        UHBC6HData Data = GMinResult[MinIdx];
        int OutputIdx = Gid.x + Gid.y * (GWidth / 4);
        
        UHBlockCompressionOutput SelectedResult;
        if (StoreBC6HMode14(Data, SelectedResult)) {}
        else if (StoreBC6HMode13(Data, SelectedResult)) {}
        else if (StoreBC6HMode12(Data, SelectedResult)) {}
        else
        {
            SelectedResult = StoreBC6HMode11(Data);
        }
        GResult[OutputIdx] = SelectedResult;
    }
}