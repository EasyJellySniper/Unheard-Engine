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

	// BC6H color, 16 bytes per block, UHE uses mode 11 implementation for now
	struct UHColorBC6HMode11
	{
		UHColorBC6HMode11()
		{
			memset(Data, 0, sizeof(uint64_t) * ARRAYSIZE(Data));
		}

		uint64_t Data[2];

		// use bit operation to store data
		//uint64_t ModeBits : 5;

		//uint64_t Color0_R : 10;
		//uint64_t Color0_G : 10;
		//uint64_t Color0_B : 10;

		//uint64_t Color1_R : 10;
		//uint64_t Color1_G : 10;
		//uint64_t Color1_B : 10;

		//uint64_t PartitionIndices : 63;
	};

	// compression
	std::vector<uint64_t> CompressBC1(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC3(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC4(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC5(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
	std::vector<uint64_t> CompressBC6H(const uint32_t Width, const uint32_t Height, const std::vector<uint8_t>& Input);
}
#endif