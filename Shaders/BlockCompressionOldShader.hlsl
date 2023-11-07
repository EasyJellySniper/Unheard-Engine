#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

// shader implementation of old block compression (BC1~5) in UHE
#define BLOCK_SIZE 4
static const float GOneDivide255 = 0.0039215686274509803921568627451f;

struct UHBlockCompressionOutput
{
    uint LowBits;
    uint HighBits;
};

RWStructuredBuffer<UHBlockCompressionOutput> GResult : register(u0);
// it's fine to define parameter with the same register number, just don't use them at the same time
StructuredBuffer<float3> GColorInput : register(t1);
StructuredBuffer<uint> GAlphaInput : register(t1);
cbuffer BlockCompressionConstant : register(b2)
{
    uint GWidth;
    uint GHeight;
    bool GIsBC3;
}

groupshared float3 GBlockColors[16];
groupshared uint GBlockAlphas[16];
groupshared float GMinError[16];
groupshared UHBlockCompressionOutput GMinResult[16];

UHBlockCompressionOutput EvaluateBC1(float3 Color0, float3 Color1, out float OutMinDiff)
{
    float3 RefColor[4];
    RefColor[0] = Color0;
    RefColor[1] = Color1;
    RefColor[2] = 0;
    RefColor[3] = 0;
    OutMinDiff = 0;
    
    // prefer Color0 > Color1 if it's BC1
    if (!GIsBC3)
    {
        if (length(RefColor[1]) > length(RefColor[0]))
        {
            RefColor[0] = Color1;
            RefColor[1] = Color0;
        }
    }

	// store color0 and color1 as RGB565
    uint RCache[2];
    uint GCache[2];
    uint BCache[2];

    int BitShiftStart = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;
    
    for (int Idx = 0; Idx < 2; Idx++)
    {
        RCache[Idx] = min(RefColor[Idx].r * GOneDivide255 * 31.0f + 0.5f, 31.0f);
        GCache[Idx] = min(RefColor[Idx].g * GOneDivide255 * 63.0f + 0.5f, 63.0f);
        BCache[Idx] = min(RefColor[Idx].b * GOneDivide255 * 31.0f + 0.5f, 31.0f);
        
        Result.LowBits |= (RCache[Idx] << 11) << BitShiftStart;
        Result.LowBits |= (GCache[Idx] << 5) << BitShiftStart;
        Result.LowBits |= BCache[Idx] << BitShiftStart;
        BitShiftStart += 16;
    }
    // end of low bits

	// intepolate color_2 and color_3, need to use uint index for condition
    if (RCache[0] > RCache[1])
    {
        RefColor[2].r = RefColor[0].r * 0.667f + RefColor[1].r * 0.333f;
        RefColor[3].r = RefColor[1].r * 0.667f + RefColor[0].r * 0.333f;
    }
    else
    {
        RefColor[2].r = (RefColor[0].r + RefColor[1].r) * 0.5f;
    }

    if (GCache[0] > GCache[1])
    {
        RefColor[2].g = RefColor[0].g * 0.667f + RefColor[1].g * 0.333f;
        RefColor[3].g = RefColor[1].g * 0.667f + RefColor[0].g * 0.333f;
    }
    else
    {
        RefColor[2].g = (RefColor[0].g + RefColor[1].g) * 0.5f;
    }

    if (BCache[0] > BCache[1])
    {
        RefColor[2].b = RefColor[0].b * 0.667f + RefColor[1].b * 0.333f;
        RefColor[3].b = RefColor[1].b * 0.667f + RefColor[0].b * 0.333f;
    }
    else
    {
        RefColor[2].b = (RefColor[0].b + RefColor[1].b) * 0.5f;
    }

	// store indices from LSB to MSB
    BitShiftStart = 0;
    for (Idx = 0; Idx < 16; Idx++)
    {
        float MinDiff = UH_FLOAT_MAX;
        uint ClosestIdx = 0;
        
        for (uint Jdx = 0; Jdx < 4; Jdx++)
        {
            float Diff = length(GBlockColors[Idx] - RefColor[Jdx]);
            if (Diff < MinDiff)
            {
                MinDiff = Diff;
                ClosestIdx = Jdx;
            }
        }

        OutMinDiff += MinDiff;
        Result.HighBits |= ClosestIdx << BitShiftStart;
        BitShiftStart += 2;
    }

    return Result;
}

// block compress for color part
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BlockCompressColor(uint3 DTid : SV_DispatchThreadID, uint GIndex : SV_GroupIndex, uint3 Gid : SV_GroupID)
{    
    // store block colors
    uint TextureIndex = DTid.x + DTid.y * GWidth;
    GBlockColors[GIndex] = GColorInput[TextureIndex];
    GMinError[GIndex] = UH_FLOAT_MAX;
    GroupMemoryBarrierWithGroupSync();
    
    if (DTid.x >= GWidth || DTid.y >= GHeight)
    {
        return;
    }
    
    // find min/max color
    float3 MaxColor = 0;
    float3 MinColor = 255.0f;
    for (uint Idx = 0; Idx < 16; Idx++)
    {
        MaxColor = max(MaxColor, GBlockColors[Idx]);
        MinColor = min(MinColor, GBlockColors[Idx]);
    }
    
    // Test the pairs that has the minimal errors except itself
    float BC1MinError = UH_FLOAT_MAX;
    float MinError = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;
    UHBlockCompressionOutput FinalResult = (UHBlockCompressionOutput)0;
    
    // on CPU side a thread runs 120 tests
    // here, only need to run 15 tests for each thread, I'll choose the minimal result across all threads later
    // if it's BC3, skip the search as we only get 3 color reference actaully, the search won't work well
    if (!GIsBC3)
    {
        for (Idx = 0; Idx < 16; Idx++)
        {
            if (Idx == GIndex)
            {
                continue;
            }
        
            Result = EvaluateBC1(GBlockColors[GIndex], GBlockColors[Idx], MinError);
            if (MinError < BC1MinError)
            {
                BC1MinError = MinError;
                FinalResult = Result;
            }
        }
    }

    if (GIsBC3)
    {
        // BC3 can only work well with this now...thinking of other method in the future
        Result = EvaluateBC1(MinColor, MaxColor, MinError);
    }
    else
    {
        Result = EvaluateBC1(MaxColor, MinColor, MinError);
    }
    
    if (MinError < BC1MinError)
    {
        BC1MinError = MinError;
        FinalResult = Result;
    }
    
    GMinError[GIndex] = BC1MinError;
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
        
        int OutputIdx = Gid.x + Gid.y * (GWidth / 4);
        GResult[OutputIdx] = GMinResult[MinIdx];
    }
}

UHBlockCompressionOutput EvaluateBC3(uint Alpha0, uint Alpha1, out float OutMinDiff)
{
    // prefer Alpha0 > Alpha1
    if (Alpha1 > Alpha0)
    {
        uint Temp = Alpha1;
        Alpha1 = Alpha0;
        Alpha0 = Temp;
    }
    
    float RefAlpha[8];
    RefAlpha[0] = (float)Alpha0;
    RefAlpha[1] = (float)Alpha1;
    OutMinDiff = 0;
    
	// interpolate alpha 2 ~ alpha 8 based on the condition
    if (Alpha0 > Alpha1)
    {
        for (int Idx = 2; Idx < 8; Idx++)
        {
            RefAlpha[Idx] = (RefAlpha[0] * (8 - Idx) + RefAlpha[1] * (Idx - 1)) / 7.0f;
        }
    }
    else
    {
        for (int Idx = 2; Idx < 6; Idx++)
        {
            RefAlpha[Idx] = (RefAlpha[0] * (6 - Idx) + RefAlpha[1] * (Idx - 1)) / 5.0f;
        }
        RefAlpha[6] = 0.0f;
        RefAlpha[7] = 255.0f;
    }

	// store alpha_0 and alpha_1
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;
    Result.LowBits |= Alpha0;
    Result.LowBits |= Alpha1 << 8;

	// store indices from LSB to MSB
    int BitShiftStart = 16;
    for (int Idx = 0; Idx < 16; Idx++)
    {
        float MinDiff = UH_FLOAT_MAX;
        uint ClosestIdx = 0;
        for (uint Jdx = 0; Jdx < 8; Jdx++)
        {
            float Diff = abs((float)GBlockAlphas[Idx] - RefAlpha[Jdx]);
            if (Diff < MinDiff)
            {
                MinDiff = Diff;
                ClosestIdx = Jdx;
            }
        }

        OutMinDiff += MinDiff;
        if (Idx < 5)
        {
            Result.LowBits |= ClosestIdx << BitShiftStart;
            BitShiftStart += 3;
        }
        else if (Idx == 5)
        {
            // the 6th index is hitting the end of low bits, carefully set it
            Result.LowBits |= ClosestIdx << BitShiftStart;
            Result.HighBits = (ClosestIdx >> 1);
            BitShiftStart = 2;
        }
        else
        {
            Result.HighBits |= ClosestIdx << BitShiftStart;
            BitShiftStart += 3;
        }
    }

    return Result;
}

// block compress for alpha part, input will be RGB anyway, I simply pick .r here
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BlockCompressAlpha(uint3 DTid : SV_DispatchThreadID, uint GIndex : SV_GroupIndex, uint3 Gid : SV_GroupID)
{    
    // store block alphas and reset GMinError
    uint TextureIndex = DTid.x + DTid.y * GWidth;
    GBlockAlphas[GIndex] = GAlphaInput[TextureIndex];
    GMinError[GIndex] = UH_FLOAT_MAX;
    GroupMemoryBarrierWithGroupSync();
    
    if (DTid.x >= GWidth || DTid.y >= GHeight)
    {
        return;
    }
    
    // find min/max alpha
    uint MaxAlpha = 0;
    uint MinAlpha = 255;
    for (uint Idx = 0; Idx < 16; Idx++)
    {
        MaxAlpha = max(MaxAlpha, GBlockAlphas[Idx]);
        MinAlpha = min(MinAlpha, GBlockAlphas[Idx]);
    }
    
    // Test the pairs that has the minimal errors except itself
    float BC3MinError = UH_FLOAT_MAX;
    float MinError = 0;
    UHBlockCompressionOutput Result = (UHBlockCompressionOutput)0;
    UHBlockCompressionOutput FinalResult = (UHBlockCompressionOutput)0;
    
    // on CPU side a thread runs 120 tests
    // here, only need to run 15 tests for each thread, I'll choose the minimal result across all threads later
    for (Idx = 0; Idx < 16; Idx++)
    {
        if (Idx == GIndex)
        {
            continue;
        }
        
        Result = EvaluateBC3(GBlockAlphas[GIndex], GBlockAlphas[Idx], MinError);
        if (MinError < BC3MinError)
        {
            BC3MinError = MinError;
            FinalResult = Result;
        }
    }

    Result = EvaluateBC3(MaxAlpha, MinAlpha, MinError);
    if (MinError < BC3MinError)
    {
        BC3MinError = MinError;
        FinalResult = Result;
    }
    
    GMinError[GIndex] = BC3MinError;
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
        
        int OutputIdx = Gid.x + Gid.y * (GWidth / 4);
        GResult[OutputIdx] = GMinResult[MinIdx];
    }
}