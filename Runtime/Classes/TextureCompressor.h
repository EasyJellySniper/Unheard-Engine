#pragma once
#include "../UnheardEngine.h"

#if WITH_DEBUG
namespace UHTextureCompressor
{
	// helper structure for readability, can be move somewhere else if they're useful
	// 
	// BC1 color, 8 bytes per block
	struct UHColorBC1
	{
		UHColorBC1()
			: Indices(0)
		{
			for (int32_t Idx = 0; Idx < 2; Idx++)
			{
				Color[Idx] = 0;
			}
		}

		uint16_t Color[2];
		uint32_t Indices;
	};

	// BC3 color, 16 bytes per block, the alpha part only since the color part is the same as BC1
	struct UHColorBC3
	{
		UHColorBC3()
			: Alpha0(0)
			, Alpha1(0)
		{
			for (int32_t Idx = 0; Idx < 6; Idx++)
			{
				AlphaIndices[Idx] = 0;
			}
		}

		uint8_t Alpha0;
		uint8_t Alpha1;

		// since each alpha indice needs 3-bit, it's tricky to set the bit with this array
		// it can be stored as uint64_t first, then memcpy to this array
		uint8_t AlphaIndices[6];
	};

	// compression
	std::vector<uint64_t> CompressBC1(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC3(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
}
#endif