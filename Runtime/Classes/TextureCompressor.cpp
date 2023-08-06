#include "TextureCompressor.h"

#if WITH_DEBUG
#include <assert.h>
#include "Types.h"
#include <array>
#include "Utility.h"
#include <unordered_map>
#include <thread>

namespace UHTextureCompressor
{
	const uint32_t GCompressThreadCount = 4;
	std::thread GCompressTasks[GCompressThreadCount];
	const float GOneDivide255 = 0.0039215686274509803921568627451f;
	// compress raw texture data to block compression, implementation follows the Microsoft document
	// https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression
	// input is assumed as RGBA8888 for now

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

		// there is a chance that minimal method doesn't work the best, use min/max method for such case
		Result = EvaluateBC1(BlockColors, MaxColor, MinColor, MinDiff);
		if (MinDiff < BC1MinDiff)
		{
			FinalResult = Result;
		}

		uint64_t OutResult;
		memcpy(&OutResult, &FinalResult, sizeof(UHColorBC1));
		return OutResult;
	}

	uint64_t BlockCompressionAlpha(const std::vector<uint32_t>& Alpha8, const uint32_t Width, const uint32_t Height, const int32_t StartY, const int32_t StartX)
	{
		uint32_t BlockAlphas[16] = { 0 };
		uint32_t RefAlpha[8] = { 0 };
		RefAlpha[1] = 255;
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

				RefAlpha[0] = std::max(RefAlpha[0], Alpha);
				RefAlpha[1] = std::min(RefAlpha[1], Alpha);
				BlockAlphas[AlphaCount++] = Alpha;
			}
		}

		// interpolate alpha 2 ~ alpha 8 based on the condition
		if (RefAlpha[0] > RefAlpha[1])
		{
			for (int32_t Idx = 2; Idx < 8; Idx++)
			{
				RefAlpha[Idx] = (RefAlpha[0] * (8 - Idx) + RefAlpha[1] * (Idx - 1)) / 7;
			}
		}
		else
		{
			for (int32_t Idx = 2; Idx < 6; Idx++)
			{
				RefAlpha[Idx] = (RefAlpha[0] * (6 - Idx) + RefAlpha[1] * (Idx - 1)) / 5;
			}
			RefAlpha[6] = 0;
			RefAlpha[7] = 1;
		}

		// store alpha_0 and alpha_1
		UHColorBC3 Result;
		Result.Alpha0 = static_cast<uint8_t>(RefAlpha[0]);
		Result.Alpha1 = static_cast<uint8_t>(RefAlpha[1]);

		// store indices from LSB to MSB
		int32_t BitShiftStart = 0;
		uint64_t Indices = 0;
		for (uint32_t Idx = 0; Idx < 16; Idx++)
		{
			int32_t MinDiff = UINT32_MAX;
			uint64_t ClosestIdx = 0;
			for (uint32_t Jdx = 0; Jdx < 8; Jdx++)
			{
				int32_t Diff = std::abs((int32_t)BlockAlphas[Idx] - (int32_t)RefAlpha[Jdx]);
				if (Diff < MinDiff)
				{
					MinDiff = Diff;
					ClosestIdx = Jdx;
				}
			}

			Indices |= ClosestIdx << BitShiftStart;
			BitShiftStart += 3;
		}

		// copy 48-bit to result
		memcpy(&Result.AlphaIndices[0], &Indices, sizeof(uint8_t) * 6);

		uint64_t OutResult;
		memcpy(&OutResult, &Result, sizeof(UHColorBC3));
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
		size_t RawColorStride = 4;
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

		size_t RawColorStride = 4;
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
}
#endif