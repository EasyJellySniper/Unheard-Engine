#include "TextureCompressor.h"

#if WITH_EDITOR
#include <assert.h>
#include "Types.h"
#include <array>
#include "Utility.h"
#include <unordered_map>
#include <thread>
#define IMATH_HALF_NO_LOOKUP_TABLE
#include <ImfRgba.h>

namespace UHTextureCompressor
{
	const uint32_t GCompressThreadCount = 4;
	std::thread GCompressTasks[GCompressThreadCount];
	const float GOneDivide255 = 0.0039215686274509803921568627451f;
	// compress raw texture data to block compression, implementation follows the Microsoft document
	// https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression
	// input is assumed as RGBA8888 for non-HDR, and RGBAHalf for HDR

	UHColorBC1 EvaluateBC1(const UHColorRGB BlockColors[16], const UHColorRGB& Color0, const UHColorRGB& Color1, float& OutMinDiff)
	{
		UHColorRGB RefColor[4];
		RefColor[0] = Color0;
		RefColor[1] = Color1;
		OutMinDiff = 0;

		// store color0 and color1 as RGB565
		uint16_t RCache[2] = { 0 };
		uint16_t GCache[2] = { 0 };
		uint16_t BCache[2] = { 0 };

		UHColorBC1 Result;
		for (int32_t Idx = 0; Idx < 2; Idx++)
		{
			RCache[Idx] = std::min((uint16_t)(RefColor[Idx].R * GOneDivide255 * 31.0f + 0.5f), (uint16_t)31);
			GCache[Idx] = std::min((uint16_t)(RefColor[Idx].G * GOneDivide255 * 63.0f + 0.5f), (uint16_t)63);
			BCache[Idx] = std::min((uint16_t)(RefColor[Idx].B * GOneDivide255 * 31.0f + 0.5f), (uint16_t)31);

			Result.Color[Idx] |= RCache[Idx] << 11;
			Result.Color[Idx] |= GCache[Idx] << 5;
			Result.Color[Idx] |= BCache[Idx];
		}

		// intepolate color_2 and color_3, need to use uint index for condition
		if (RCache[0] > RCache[1])
		{
			RefColor[2].R = RefColor[0].R * 0.667f + RefColor[1].R * 0.333f;
			RefColor[3].R = RefColor[1].R * 0.667f + RefColor[0].R * 0.333f;
		}
		else
		{
			RefColor[2].R = (RefColor[0].R + RefColor[1].R) * 0.5f;
		}

		if (GCache[0] > GCache[1])
		{
			RefColor[2].G = RefColor[0].G * 0.667f + RefColor[1].G * 0.333f;
			RefColor[3].G = RefColor[1].G * 0.667f + RefColor[0].G * 0.333f;
		}
		else
		{
			RefColor[2].G = (RefColor[0].G + RefColor[1].G) * 0.5f;
		}

		if (BCache[0] > BCache[1])
		{
			RefColor[2].B = RefColor[0].B * 0.667f + RefColor[1].B * 0.333f;
			RefColor[3].B = RefColor[1].B * 0.667f + RefColor[0].B * 0.333f;
		}
		else
		{
			RefColor[2].B = (RefColor[0].B + RefColor[1].B) * 0.5f;
		}

		// store indices from LSB to MSB
		int32_t BitShiftStart = 0;
		for (uint32_t Idx = 0; Idx < 16; Idx++)
		{
			float MinDiff = std::numeric_limits<float>::max();
			uint32_t ClosestIdx = 0;
			for (uint32_t Jdx = 0; Jdx < 4; Jdx++)
			{
				const float Diff = sqrtf(UHColorRGB::SquareDiff(BlockColors[Idx], RefColor[Jdx]));
				if (Diff < MinDiff)
				{
					MinDiff = Diff;
					ClosestIdx = Jdx;
				}
			}

			OutMinDiff += MinDiff;
			Result.Indices |= ClosestIdx << BitShiftStart;
			BitShiftStart += 2;
		}

		return Result;
	}

	uint64_t BlockCompressionColor(const std::vector<UHColorRGB>& RGB888, const uint32_t Width, const uint32_t Height, const int32_t StartY, const int32_t StartX)
	{
		// collect the block colors and find the min/max color
		UHColorRGB BlockColors[16];
		uint32_t ColorCount = 0;
		UHColorRGB MaxColor;
		UHColorRGB MinColor(255, 255, 255);

		for (int32_t Y = StartY; Y < StartY + 4; Y++)
		{
			for (int32_t X = StartX; X < StartX + 4; X++)
			{
				// clamp to nearest valid index
				const int32_t NX = std::clamp(X, 0, (int32_t)Width - 1);
				const int32_t NY = std::clamp(Y, 0, (int32_t)Height - 1);
				const int32_t ColorIdx = Width * NY + NX;
				const UHColorRGB& Color = RGB888[ColorIdx];
				BlockColors[ColorCount++] = Color;

				if (Color.Lumin() > MaxColor.Lumin())
				{
					MaxColor = Color;
				}
				
				if (Color.Lumin() < MinColor.Lumin())
				{
					MinColor = Color;
				}
			}
		}

		// brute-force method, test all combination of color reference and use the result that has the smallest BC1MinDiff
		float BC1MinDiff = std::numeric_limits<float>::max();
		UHColorBC1 FinalResult;
		UHColorBC1 Result;
		float MinDiff;

		for (int32_t Idx = 0; Idx < 16; Idx++)
		{
			for (int32_t Jdx = Idx + 1; Idx < 16; Idx++)
			{
				Result = EvaluateBC1(BlockColors, BlockColors[Idx], BlockColors[Jdx], MinDiff);
				if (MinDiff < BC1MinDiff)
				{
					BC1MinDiff = MinDiff;
					FinalResult = Result;
				}
			}
		}

		// there is a chance that minimal distance method doesn't work the best, use min/max method for such case
		Result = EvaluateBC1(BlockColors, MaxColor, MinColor, MinDiff);
		if (MinDiff < BC1MinDiff)
		{
			FinalResult = Result;
		}

		uint64_t OutResult;
		memcpy_s(&OutResult, sizeof(UHColorBC1), &FinalResult, sizeof(UHColorBC1));
		return OutResult;
	}

	// evaludate bc3, almost the same algorithem as BC1
	UHColorBC3 EvaluateBC3(const uint32_t BlockAlphas[16], const uint32_t& Alpha0, const uint32_t& Alpha1, float& OutMinDiff)
	{
		float RefAlpha[8];
		RefAlpha[0] = static_cast<float>(Alpha0);
		RefAlpha[1] = static_cast<float>(Alpha1);
		OutMinDiff = 0;

		// interpolate alpha 2 ~ alpha 8 based on the condition, use float for better precision
		// but still using uint alpha as the condition
		if (Alpha0 > Alpha1)
		{
			for (int32_t Idx = 2; Idx < 8; Idx++)
			{
				RefAlpha[Idx] = (RefAlpha[0] * (8 - Idx) + RefAlpha[1] * (Idx - 1)) / 7.0f;
			}
		}
		else
		{
			for (int32_t Idx = 2; Idx < 6; Idx++)
			{
				RefAlpha[Idx] = (RefAlpha[0] * (6 - Idx) + RefAlpha[1] * (Idx - 1)) / 5.0f;
			}
			RefAlpha[6] = 0.0f;
			RefAlpha[7] = 1.0f;
		}

		// store alpha_0 and alpha_1
		UHColorBC3 Result;
		Result.Alpha0 = static_cast<uint8_t>(Alpha0);
		Result.Alpha1 = static_cast<uint8_t>(Alpha1);

		// store indices from LSB to MSB
		int32_t BitShiftStart = 0;
		uint64_t Indices = 0;
		for (uint32_t Idx = 0; Idx < 16; Idx++)
		{
			float MinDiff = std::numeric_limits<float>::max();
			uint64_t ClosestIdx = 0;
			for (uint32_t Jdx = 0; Jdx < 8; Jdx++)
			{
				float Diff = std::abs((float)BlockAlphas[Idx] - RefAlpha[Jdx]);
				if (Diff < MinDiff)
				{
					MinDiff = Diff;
					ClosestIdx = Jdx;
				}
			}

			OutMinDiff += MinDiff;
			Indices |= ClosestIdx << BitShiftStart;
			BitShiftStart += 3;
		}

		// copy 48-bit to result
		memcpy_s(&Result.AlphaIndices[0], sizeof(uint8_t) * 6, &Indices, sizeof(uint8_t) * 6);

		return Result;
	}

	uint64_t BlockCompressionAlpha(const std::vector<uint32_t>& Alpha8, const uint32_t Width, const uint32_t Height, const int32_t StartY, const int32_t StartX)
	{
		uint32_t BlockAlphas[16] = { 0 };
		uint32_t MaxAlpha = 0;
		uint32_t MinAlpha = 255;
		uint32_t AlphaCount = 0;

		// for alpha compression, it has more indices bits than color, min-max method should be good enough
		for (int32_t Y = StartY; Y < StartY + 4; Y++)
		{
			for (int32_t X = StartX; X < StartX + 4; X++)
			{
				// clamp to nearest valid index
				const int32_t NX = std::clamp(X, 0, (int32_t)Width - 1);
				const int32_t NY = std::clamp(Y, 0, (int32_t)Height - 1);
				const int32_t AlphaIdx = Width * NY + NX;
				const uint32_t& Alpha = Alpha8[AlphaIdx];

				MaxAlpha = std::max(MaxAlpha, Alpha);
				MinAlpha = std::min(MinAlpha, Alpha);
				BlockAlphas[AlphaCount++] = Alpha;
			}
		}

		// brute-force method, test all combination of color reference and use the result that has the smallest BC3MinDiff
		float BC3MinDiff = std::numeric_limits<float>::max();
		UHColorBC3 FinalResult;
		UHColorBC3 Result;
		float MinDiff;

		for (int32_t Idx = 0; Idx < 16; Idx++)
		{
			for (int32_t Jdx = Idx + 1; Idx < 16; Idx++)
			{
				Result = EvaluateBC3(BlockAlphas, BlockAlphas[Idx], BlockAlphas[Jdx], MinDiff);
				if (MinDiff < BC3MinDiff)
				{
					BC3MinDiff = MinDiff;
					FinalResult = Result;
				}
			}
		}

		// there is a chance that minimal distance method doesn't work the best, use min/max method for such case
		Result = EvaluateBC3(BlockAlphas, MaxAlpha, MinAlpha, MinDiff);
		if (MinDiff < BC3MinDiff)
		{
			FinalResult = Result;
		}

		uint64_t OutResult;
		memcpy_s(&OutResult, sizeof(UHColorBC3), &Result, sizeof(UHColorBC3));
		return OutResult;
	}

	void WaitCompressionThreads()
	{
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			if (GCompressTasks[Tdx].joinable())
			{
				GCompressTasks[Tdx].join();
			}
		}
	}

	std::vector<uint64_t> CompressBC1(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input)
	{
		// 8 bytes per 4x4 block, BC1 compression, alpha channel will be discard
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize);

		std::vector<UHColorRGB> RGB888(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < RGB888.size(); Idx++)
		{
			RGB888[Idx].R = (float)Input[Idx * RawColorStride];
			RGB888[Idx].G = (float)Input[Idx * RawColorStride + 1];
			RGB888[Idx].B = (float)Input[Idx * RawColorStride + 2];
		}

		// assign BC tasks to threads, divided based on the height
		assert(GCompressThreadCount == 4);
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			// when height is < 8, use one thread only
			if (Height <= 8 && Tdx > 0)
			{
				break;
			}

			GCompressTasks[Tdx] = std::thread([Tdx, Width, Height, OutputSize, &RGB888, &Output]()
				{
					const uint32_t YProcessCount = Height / GCompressThreadCount;
					const uint32_t StartY = (Height > 8) ? Tdx * YProcessCount : 0;
					const uint32_t EndY = (Height > 8) ? StartY + YProcessCount : Height;

					for (uint32_t Y = StartY; Y < EndY; Y += 4)
					{
						for (uint32_t X = 0; X < Width; X += 4)
						{
							const int32_t OutputIdx = X / 4 + (Y / 4) * (Width / 4);
							if (OutputIdx < static_cast<int32_t>(OutputSize))
							{
								Output[OutputIdx] = BlockCompressionColor(RGB888, Width, Height, (int32_t)Y, (int32_t)X);
							}
						}
					}
				});
		}
		WaitCompressionThreads();

		return Output;
	}

	std::vector<uint64_t> CompressBC3(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input)
	{
		// 16 bytes per 4x4 block, BC3 compression, alpha channel will be preserved
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize * 2);

		std::vector<UHColorRGB> RGB888(Width * Height);
		std::vector<uint32_t> Alpha8(Width * Height);

		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < RGB888.size(); Idx++)
		{
			RGB888[Idx].R = (float)Input[Idx * RawColorStride];
			RGB888[Idx].G = (float)Input[Idx * RawColorStride + 1];
			RGB888[Idx].B = (float)Input[Idx * RawColorStride + 2];
			Alpha8[Idx] = Input[Idx * RawColorStride + 3];
		}

		// assign BC tasks to threads, divided based on the height
		assert(GCompressThreadCount == 4);
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			// when height is < 8, use one thread only
			if (Height <= 8 && Tdx > 0)
			{
				break;
			}

			GCompressTasks[Tdx] = std::thread([Tdx, Width, Height, OutputSize, &RGB888, &Alpha8, &Output]()
				{
					const uint32_t YProcessCount = Height / GCompressThreadCount;
					const uint32_t StartY = (Height > 8) ? Tdx * YProcessCount : 0;
					const uint32_t EndY = (Height > 8) ? StartY + YProcessCount : Height;

					for (uint32_t Y = StartY; Y < EndY; Y += 4)
					{
						for (uint32_t X = 0; X < Width; X += 4)
						{
							const int32_t OutputIdx = X / 4 + (Y / 4) * (Width / 4);
							if (OutputIdx < static_cast<int32_t>(OutputSize))
							{
								Output[2 * OutputIdx] = BlockCompressionAlpha(Alpha8, Width, Height, (int32_t)Y, (int32_t)X);
								Output[2 * OutputIdx + 1] = BlockCompressionColor(RGB888, Width, Height, (int32_t)Y, (int32_t)X);
							}
						}
					}
				});
		}
		WaitCompressionThreads();

		return Output;
	}

	std::vector<uint64_t> CompressBC4(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input)
	{
		// 8 bytes per 4x4 block, BC4 compression, stores red channel only
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize);

		std::vector<uint32_t> Red8(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < Red8.size(); Idx++)
		{
			Red8[Idx] = Input[Idx * RawColorStride];
		}

		// assign BC tasks to threads, divided based on the height
		assert(GCompressThreadCount == 4);
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			// when height is < 8, use one thread only
			if (Height <= 8 && Tdx > 0)
			{
				break;
			}

			GCompressTasks[Tdx] = std::thread([Tdx, Width, Height, OutputSize, &Red8, &Output]()
				{
					const uint32_t YProcessCount = Height / GCompressThreadCount;
					const uint32_t StartY = (Height > 8) ? Tdx * YProcessCount : 0;
					const uint32_t EndY = (Height > 8) ? StartY + YProcessCount : Height;

					for (uint32_t Y = StartY; Y < EndY; Y += 4)
					{
						for (uint32_t X = 0; X < Width; X += 4)
						{
							const int32_t OutputIdx = X / 4 + (Y / 4) * (Width / 4);
							if (OutputIdx < static_cast<int32_t>(OutputSize))
							{
								// BC4 uses the same way as BC3's alpha part to store R
								Output[OutputIdx] = BlockCompressionAlpha(Red8, Width, Height, (int32_t)Y, (int32_t)X);
							}
						}
					}
				});
		}
		WaitCompressionThreads();

		return Output;
	}

	std::vector<uint64_t> CompressBC5(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input)
	{
		// 16 bytes per 4x4 block, BC5 compression, stores red/green channel only
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize * 2);

		std::vector<uint32_t> Red8(Width * Height);
		std::vector<uint32_t> Green8(Width * Height);
		const size_t RawColorStride = 4;
		for (size_t Idx = 0; Idx < Red8.size(); Idx++)
		{
			Red8[Idx] = Input[Idx * RawColorStride];
			Green8[Idx] = Input[Idx * RawColorStride + 1];
		}

		// assign BC tasks to threads, divided based on the height
		assert(GCompressThreadCount == 4);
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			// when height is < 8, use one thread only
			if (Height <= 8 && Tdx > 0)
			{
				break;
			}

			GCompressTasks[Tdx] = std::thread([Tdx, Width, Height, OutputSize, &Red8, &Green8, &Output]()
				{
					const uint32_t YProcessCount = Height / GCompressThreadCount;
					const uint32_t StartY = (Height > 8) ? Tdx * YProcessCount : 0;
					const uint32_t EndY = (Height > 8) ? StartY + YProcessCount : Height;

					for (uint32_t Y = StartY; Y < EndY; Y += 4)
					{
						for (uint32_t X = 0; X < Width; X += 4)
						{
							const int32_t OutputIdx = X / 4 + (Y / 4) * (Width / 4);
							if (OutputIdx < static_cast<int32_t>(OutputSize))
							{
								// BC5 uses the same way as BC3's alpha part to store R/G
								Output[2 * OutputIdx] = BlockCompressionAlpha(Red8, Width, Height, (int32_t)Y, (int32_t)X);
								Output[2 * OutputIdx + 1] = BlockCompressionAlpha(Green8, Width, Height, (int32_t)Y, (int32_t)X);
							}
						}
					}
				});
		}
		WaitCompressionThreads();

		return Output;
	}

	// all quantize below is signed
	int32_t QuantizeAs10Bit(int32_t InVal)
	{
		int32_t S = 0;
		int32_t Prec = 10;
		if (InVal < 0)
		{
			S = 1;
			InVal = -InVal;
		}

		int32_t Q = (InVal << (Prec - 1)) / (0x7bff + 1);
		if (S)
		{
			Q = -Q;
		}

		return Q;
	}

	int32_t UnquantizeFrom10Bit(int32_t InVal)
	{
		int32_t S = 0;
		int32_t Prec = 10;
		if (InVal < 0)
		{
			S = 1;
			InVal = -InVal;
		}

		int32_t Q;
		if (InVal == 0)
		{
			Q = 0;
		}
		else if (InVal >= ((1 << (Prec - 1)) - 1))
		{
			Q = 0x7FFF;
		}
		else
		{
			Q = ((InVal << 15) + 0x4000) >> (Prec - 1);
		}

		if (S)
		{
			Q = -Q;
		}

		return Q;
	}

	void GetBC6HPalette(const Imf::Rgba& Color0, const Imf::Rgba& Color1, float PaletteR[16], float PaletteG[16], float PaletteB[16])
	{
		const int32_t Weights[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

		// quantize and unquantize for the best consistency
		const int32_t RefR0 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color0.r.bits()));
		const int32_t RefG0 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color0.g.bits()));
		const int32_t RefB0 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color0.b.bits()));
		const int32_t RefR1 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color1.r.bits()));
		const int32_t RefG1 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color1.g.bits()));
		const int32_t RefB1 = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)Color1.b.bits()));

		// interpolate 16 values for comparison
		for (int32_t Idx = 0; Idx < 16; Idx++)
		{
			const int32_t R = (RefR0 * (64 - Weights[Idx]) + RefR1 * Weights[Idx] + 32) >> 6;
			const int32_t G = (RefG0 * (64 - Weights[Idx]) + RefG1 * Weights[Idx] + 32) >> 6;
			const int32_t B = (RefB0 * (64 - Weights[Idx]) + RefB1 * Weights[Idx] + 32) >> 6;

			Imf::Rgba Temp;
			Temp.r = static_cast<uint16_t>(R);
			Temp.g = static_cast<uint16_t>(G);
			Temp.b = static_cast<uint16_t>(B);
			PaletteR[Idx] = Temp.r;
			PaletteG[Idx] = Temp.g;
			PaletteB[Idx] = Temp.b;
		}
	}

	// BC6H references:
	// https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc6h-format
	// https://github.com/microsoft/DirectXTex/blob/main/DirectXTex/BC6HBC7.cpp
	// Color0 and Color1 might be swapped during the process
	uint64_t EvaluateBC6H(const Imf::Rgba BlockColors[16], Imf::Rgba& Color0, Imf::Rgba& Color1, float& OutMinDiff)
	{
		// setup palette for BC6H
		float PaletteR[16];
		float PaletteG[16];
		float PaletteB[16];
		GetBC6HPalette(Color0, Color1, PaletteR, PaletteG, PaletteB);

		OutMinDiff = 0;
		int32_t BitShiftStart = 0;
		uint64_t Indices = 0;
		for (uint32_t Idx = 0; Idx < 16;)
		{
			const int32_t R = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)BlockColors[Idx].r.bits()));
			const int32_t G = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)BlockColors[Idx].g.bits()));
			const int32_t B = UnquantizeFrom10Bit(QuantizeAs10Bit((int32_t)BlockColors[Idx].b.bits()));
			Imf::Rgba Temp;
			Temp.r = static_cast<uint16_t>(R);
			Temp.g = static_cast<uint16_t>(G);
			Temp.b = static_cast<uint16_t>(B);

			float BlockR = Temp.r;
			float BlockG = Temp.g;
			float BlockB = Temp.b;

			float MinDiff = std::numeric_limits<float>::max();
			uint64_t ClosestIdx = 0;

			for (uint32_t Jdx = 0; Jdx < 16; Jdx++)
			{
				const float Diff = sqrtf((float)(BlockR - PaletteR[Jdx]) * (float)(BlockR - PaletteR[Jdx])
					+ (float)(BlockG - PaletteG[Jdx]) * (float)(BlockG - PaletteG[Jdx])
					+ (float)(BlockB - PaletteB[Jdx]) * (float)(BlockB - PaletteB[Jdx]));

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
				Imf::Rgba Temp = Color0;
				Color0 = Color1;
				Color1 = Temp;
				GetBC6HPalette(Color0, Color1, PaletteR, PaletteG, PaletteB);
				continue;
			}

			OutMinDiff += MinDiff;
			Indices |= ClosestIdx << BitShiftStart;
			BitShiftStart += (Idx == 0) ? 3 : 4;
			Idx++;
		}

		return Indices;
	}

	// assume it's signed, this function properly converts from half float to 10-bit data needed in BC6H decode
	uint64_t HalfTo10Bit(int32_t InVal)
	{
		// do the BC6H quantitation intentionally then shift the bits
		// it will be more consistent this way
		int32_t Result = QuantizeAs10Bit(InVal);
		Result = UnquantizeFrom10Bit(Result);

		return static_cast<uint64_t>(Result) >> 6;
	}

	UHColorBC6HMode11 BlockCompressionHDR(const std::vector<Imf::Rgba>& RGBHalf, const uint32_t Width, const uint32_t Height, const int32_t StartY, const int32_t StartX)
	{
		// almost the same as BlockCompressionColor() impl, but store as half
		Imf::Rgba BlockColors[16];
		uint32_t ColorCount = 0;

		UHColorRGB MaxTemp(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		UHColorRGB MinTemp(FLT_MAX, FLT_MAX, FLT_MAX);
		for (int32_t Y = StartY; Y < StartY + 4; Y++)
		{
			for (int32_t X = StartX; X < StartX + 4; X++)
			{
				// clamp to nearest valid index
				const int32_t NX = std::clamp(X, 0, (int32_t)Width - 1);
				const int32_t NY = std::clamp(Y, 0, (int32_t)Height - 1);
				const int32_t ColorIdx = Width * NY + NX;
				const Imf::Rgba& Color = RGBHalf[ColorIdx];
				BlockColors[ColorCount++] = Color;

				UHColorRGB ColorF(Color.r, Color.g, Color.b);
				if (ColorF.Lumin() > MaxTemp.Lumin())
				{
					MaxTemp = ColorF;
				}

				if (ColorF.Lumin() < MinTemp.Lumin())
				{
					MinTemp = ColorF;
				}
			}
		}
		Imf::Rgba MaxColor(MaxTemp.R, MaxTemp.G, MaxTemp.B);
		Imf::Rgba MinColor(MinTemp.R, MinTemp.G, MinTemp.B);

		// brute-force method, test all combination of color reference and use the result that has the smallest BC1MinDiff
		float BC6HMinDiff = std::numeric_limits<float>::max();
		float MinDiff;
		Imf::Rgba Color0;
		Imf::Rgba Color1;
		uint64_t Indices = 0;

		for (int32_t Idx = 0; Idx < 16; Idx++)
		{
			for (int32_t Jdx = Idx + 1; Idx < 16; Idx++)
			{
				Imf::Rgba C0 = BlockColors[Idx];
				Imf::Rgba C1 = BlockColors[Jdx];

				const uint64_t Index = EvaluateBC6H(BlockColors, C0, C1, MinDiff);
				if (MinDiff < BC6HMinDiff)
				{
					BC6HMinDiff = MinDiff;
					Color0 = C0;
					Color1 = C1;
					Indices = Index;
				}
			}
		}

		// there is a chance that minimal distance method doesn't work the best, use min/max method for such case
		const uint64_t Index = EvaluateBC6H(BlockColors, MaxColor, MinColor, MinDiff);
		if (MinDiff < BC6HMinDiff)
		{
			Color0 = MaxColor;
			Color1 = MinColor;
			Indices = Index;
		}

		// store result from LSB to MSB
		int32_t BitShiftStart = 0;
		UHColorBC6HMode11 Result{};

		// first 5 bits for mode, mode 11 is corresponding to 00011
		Result.Data[0] = 3;
		BitShiftStart += 5;

		// next 60 bits for reference colors, 10 bits for each color component
		Result.Data[0] |= HalfTo10Bit(Color0.r.bits()) << BitShiftStart;
		BitShiftStart += 10;

		Result.Data[0] |= HalfTo10Bit(Color0.g.bits()) << BitShiftStart;
		BitShiftStart += 10;

		Result.Data[0] |= HalfTo10Bit(Color0.b.bits()) << BitShiftStart;
		BitShiftStart += 10;

		Result.Data[0] |= HalfTo10Bit(Color1.r.bits()) << BitShiftStart;
		BitShiftStart += 10;

		Result.Data[0] |= HalfTo10Bit(Color1.g.bits()) << BitShiftStart;
		BitShiftStart += 10;

		// note that the second color reference blue will across the last 9 bits of Data[0] and the first bit of Data[1]
		const uint64_t Color1_B = HalfTo10Bit(Color1.b.bits());
		Result.Data[0] |= Color1_B << BitShiftStart;

		BitShiftStart = 0;
		Result.Data[1] = Color1_B >> 15;

		BitShiftStart++;
		Result.Data[1] |= Indices << BitShiftStart;

		return Result;
	}

	std::vector<uint64_t> CompressBC6H(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input)
	{
		const uint32_t OutputSize = std::max(Width * Height / 16, (uint32_t)1);
		std::vector<uint64_t> Output(OutputSize * 2);

		// collect RGB info
		std::vector<Imf::Rgba> RGBHalf(Width * Height);

		// input for BC6H should be RGBAFloat16, store as half float
		size_t Stride = sizeof(Imf::Rgba);
		for (size_t Idx = 0; Idx < RGBHalf.size(); Idx++)
		{
			Imf::Rgba RGBAHalf{};
			memcpy_s(&RGBAHalf, Stride, Input.data() + Idx * Stride, Stride);
			RGBHalf[Idx] = RGBAHalf;
		}

		// assign BC tasks to threads, divided based on the height
		assert(GCompressThreadCount == 4);
		for (uint32_t Tdx = 0; Tdx < GCompressThreadCount; Tdx++)
		{
			// when height is < 8, use one thread only
			if (Height <= 8 && Tdx > 0)
			{
				break;
			}

			GCompressTasks[Tdx] = std::thread([Tdx, Width, Height, OutputSize, &RGBHalf, &Output]()
				{
					const uint32_t YProcessCount = Height / GCompressThreadCount;
					const uint32_t StartY = (Height > 8) ? Tdx * YProcessCount : 0;
					const uint32_t EndY = (Height > 8) ? StartY + YProcessCount : Height;

					for (uint32_t Y = StartY; Y < EndY; Y += 4)
					{
						for (uint32_t X = 0; X < Width; X += 4)
						{
							const int32_t OutputIdx = X / 4 + (Y / 4) * (Width / 4);
							if (OutputIdx < static_cast<int32_t>(OutputSize))
							{
								// carefully store the result
								const UHColorBC6HMode11 Result = BlockCompressionHDR(RGBHalf, Width, Height, (int32_t)Y, (int32_t)X);
								const size_t Stride = sizeof(UHColorBC6HMode11);
								memcpy_s(Output.data() + OutputIdx * 2, Stride, &Result, Stride);
							}
						}
					}
				});
		}
		WaitCompressionThreads();

		return Output;
	}
}
#endif