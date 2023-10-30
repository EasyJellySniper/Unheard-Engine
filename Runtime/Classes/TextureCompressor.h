#pragma once
#include "../UnheardEngine.h"

#if WITH_EDITOR
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
			memset(Color, 0, sizeof(uint16_t) * ARRAYSIZE(Color));
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
			memset(AlphaIndices, 0, sizeof(uint8_t) * ARRAYSIZE(AlphaIndices));
		}

		uint8_t Alpha0;
		uint8_t Alpha1;

		// since each alpha indice needs 3-bit, it's tricky to set the bit with this array
		// it can be stored as uint64_t first, then memcpy to this array
		uint8_t AlphaIndices[6];
	};

	// BC6H color, 16 bytes per block, the data layout depends on the implementation
	// there are 14 modes for BC6H, each one uses different data layout
	struct UHColorBC6H
	{
		UHColorBC6H()
		{
			LowBits = 0;
			HighBits = 0;
		}

		uint64_t LowBits;
		uint64_t HighBits;
	};

	// compression
	std::vector<uint64_t> CompressBC1(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC3(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC4(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC5(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC6H(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
}
#endif