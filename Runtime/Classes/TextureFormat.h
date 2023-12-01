#pragma once
#include "../../UnheardEngine.h"

// high-level definition of the texture format in UHE
// these should be translated to low-level formats
enum UHTextureFormat
{
	UH_FORMAT_NONE = 0,
	UH_FORMAT_RGBA8_UNORM,
	UH_FORMAT_RGBA8_SRGB,
	UH_FORMAT_RGBA16F,
	UH_FORMAT_BGRA8_SRGB,
	UH_FORMAT_BGRA8_UNORM,
	UH_FORMAT_RGB32F,
	UH_FORMAT_D16,
	UH_FORMAT_D24_S8,
	UH_FORMAT_D32F,
	UH_FORMAT_D32F_S8,
	UH_FORMAT_X8_D24,
	UH_FORMAT_A2B10G10R10,
	UH_FORMAT_A2R10G10B10,
	UH_FORMAT_RG16F,
	UH_FORMAT_R8_UNORM,
	UH_FORMAT_R16F,

	UH_FORMAT_BC1_UNORM,
	UH_FORMAT_BC1_SRGB,
	UH_FORMAT_BC3_UNORM,
	UH_FORMAT_BC3_SRGB,
	UH_FORMAT_BC4,
	UH_FORMAT_BC5,
	UH_FORMAT_BC6H,

	UH_FORMAT_R16_UNORM,
	UH_FORMAT_RG16_UNORM,
	UH_FORMAT_R11G11B10,

	// add the format above
	UH_FORMAT_MAX
};

struct UHTextureFormatData
{
	UHTextureFormatData(int32_t InByteSize, int32_t InBlockSize, int32_t InNumChannels, bool bInCanGenerateMipmap)
		: ByteSize(InByteSize)
		, BlockSize(InBlockSize)
		, NumChannels(InNumChannels)
		, bCanGenerateMipmap(bInCanGenerateMipmap)
	{
	}

	int32_t ByteSize;

	// multiplied block size
	int32_t BlockSize;

	int32_t NumChannels;

	bool bCanGenerateMipmap;
};

// texture format data, their info must match the format definition
inline const UHTextureFormatData GTextureFormatData[UH_FORMAT_MAX] =
{
	{0,0,0,true},
	{4,1,4,true},
	{4,1,4,true},
	{8,1,4,true},
	{4,1,4,true},
	{4,1,4,true},
	{12,1,3,true},
	{2,1,1,true},
	{4,1,2,true},
	{4,1,1,true},
	{8,1,2,true},
	{4,1,1,true},
	{4,1,4,true},
	{4,1,4,true},
	{4,1,2,true},
	{1,1,1,true},
	{2,1,1,true},

	{8,16,3,false},
	{8,16,3,false},
	{16,16,4,false},
	{16,16,4,false},
	{8,16,1,false},
	{16,16,2,false},
	{16,16,3,false},

	{2,1,1,true},
	{4,1,2,true},
	{4,1,3,true},
};

extern VkFormat GetVulkanFormat(UHTextureFormat InUHFormat);