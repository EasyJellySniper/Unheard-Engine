#include "Texture2D.h"
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "AssetPath.h"
#include "../Engine/Graphic.h"
#include "../Renderer/RenderBuilder.h"
#include "TextureCompressor.h"

UHTexture2D::UHTexture2D()
	: UHTexture2D("", "", VkExtent2D(), UH_FORMAT_RGBA8_SRGB, UHTextureSettings())
{

}

UHTexture2D::UHTexture2D(std::string InName, std::string InSourcePath, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings)
	: UHTexture(InName, InExtent, InFormat, InSettings)
	, bSharedMemory(true)
{
	SourcePath = InSourcePath;
	TextureType = Texture2D;
}

void UHTexture2D::ReleaseCPUTextureData()
{
	TextureData.clear();
	for (UHRenderBuffer<uint8_t>& Buffer : RawStageBuffers)
	{
		Buffer.Release();
	}
}

bool UHTexture2D::Import(std::filesystem::path InTexturePath)
{
	if (InTexturePath.extension() != GTextureAssetExtension)
	{
		return false;
	}

	std::ifstream FileIn(InTexturePath.string().c_str(), std::ios::in | std::ios::binary);
	if (!FileIn.is_open())
	{
		UHE_LOG(L"Failed to Load UHTexture " + InTexturePath.wstring() + L"!\n");
		return false;
	}

	// read version and type
	FileIn.read(reinterpret_cast<char*>(&TextureVersion), sizeof(TextureVersion));
	FileIn.read(reinterpret_cast<char*>(&TextureType), sizeof(TextureType));

	// read texture name, source path
	UHUtilities::ReadStringData(FileIn, Name);
	UHUtilities::ReadStringData(FileIn, SourcePath);
	UHUtilities::ReadStringData(FileIn, RawSourcePath);

	// read extent
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.height), sizeof(ImageExtent.height));

	// read texture data
	UHUtilities::ReadVectorData(FileIn, TextureData);

	// read texture settings
	FileIn.read(reinterpret_cast<char*>(&TextureSettings), sizeof(TextureSettings));

	FileIn.close();

	return true;
}

#if WITH_EDITOR
void UHTexture2D::Recreate(bool bNeedGeneratMipmap)
{
	GfxCache->WaitGPU();
	for (UHRenderBuffer<uint8_t>& Buffer : RawStageBuffers)
	{
		Buffer.Release();
	}
	Release();
	TextureSettings.bIsCompressed = false;
	CreateTexture(bSharedMemory);

	// upload the 1st slice and generate mip map
	VkCommandBuffer UploadCmd = GfxCache->BeginOneTimeCmd();
	UHRenderBuilder UploadBuilder(GfxCache, UploadCmd);
	UploadToGPU(GfxCache, UploadCmd, UploadBuilder);
	if (bNeedGeneratMipmap)
	{
		GenerateMipMaps(GfxCache, UploadCmd, UploadBuilder);
	}
	GfxCache->EndOneTimeCmd(UploadCmd);


	// readback CPU data for storage, this includes the mip map data
	TextureData = ReadbackTextureData();

	// since the mip map generation can't be done in block compression, always process compression after raw data is done
	const int32_t RawByteSize = (TextureSettings.bIsHDR) ? GTextureFormatData[UH_FORMAT_RGBA16F].ByteSize : GTextureFormatData[UH_FORMAT_RGBA8_UNORM].ByteSize;
	if (TextureSettings.CompressionSetting != CompressionNone)
	{
		std::vector<uint64_t> CompressedData;
		uint64_t MipStartIndex = 0;
		uint64_t MipEndIndex = ImageExtent.width * ImageExtent.height * RawByteSize;

		for (uint32_t Idx = 0; Idx < GetMipMapCount(); Idx++)
		{
			std::vector<uint8_t> MipData(TextureData.begin() + MipStartIndex, TextureData.begin() + MipEndIndex);
			std::vector<uint64_t> CompressedMipData;

			switch (TextureSettings.CompressionSetting)
			{
			case BC1:
				CompressedMipData = UHTextureCompressor::CompressBC1(ImageExtent.width >> Idx, ImageExtent.height >> Idx, MipData);
				break;

			case BC3:
				CompressedMipData = UHTextureCompressor::CompressBC3(ImageExtent.width >> Idx, ImageExtent.height >> Idx, MipData);
				break;

			case BC4:
				CompressedMipData = UHTextureCompressor::CompressBC4(ImageExtent.width >> Idx, ImageExtent.height >> Idx, MipData);
				break;

			case BC5:
				CompressedMipData = UHTextureCompressor::CompressBC5(ImageExtent.width >> Idx, ImageExtent.height >> Idx, MipData);
				break;

			case BC6H:
				CompressedMipData = UHTextureCompressor::CompressBC6H(ImageExtent.width >> Idx, ImageExtent.height >> Idx, MipData);
				break;

			default:
				break;
			}

			CompressedData.insert(CompressedData.end(), CompressedMipData.begin(), CompressedMipData.end());

			MipStartIndex = MipEndIndex;
			MipEndIndex += (ImageExtent.width >> (Idx + 1)) * (ImageExtent.height >> (Idx + 1)) * RawByteSize;
		}

		// convert to uint8 array
		const size_t OutputSize = CompressedData.size() * sizeof(uint64_t);
		TextureData.clear();
		TextureData.resize(OutputSize);
		memcpy_s(TextureData.data(), OutputSize, CompressedData.data(), OutputSize);
		TextureSettings.bIsCompressed = true;

		// repeat the texture creation for compressed texture
		for (UHRenderBuffer<uint8_t>& Buffer : RawStageBuffers)
		{
			Buffer.Release();
		}
		Release();
		CreateTexture(bSharedMemory);

		// upload compressed data
		VkCommandBuffer UploadCmd = GfxCache->BeginOneTimeCmd();
		UHRenderBuilder UploadBuilder(GfxCache, UploadCmd);
		UploadToGPU(GfxCache, UploadCmd, UploadBuilder);
		GfxCache->EndOneTimeCmd(UploadCmd);
	}

	if (!bNeedGeneratMipmap)
	{
		bIsMipMapGenerated = true;
	}
}

std::vector<uint8_t> UHTexture2D::ReadbackTextureData()
{
	uint32_t MipCount = GetMipMapCount();
	std::vector<uint8_t> NewData;
	std::vector<UHRenderBuffer<uint8_t>> ReadbackBuffer(MipCount);

	VkCommandBuffer ReadbackCmd = GfxCache->BeginOneTimeCmd();
	UHRenderBuilder ReadbackBuilder(GfxCache, ReadbackCmd);

	// readback per mip slice
	const uint32_t ByteSize = TextureSettings.bIsHDR ? 8 : 4;
	for (uint32_t MipIdx = 0; MipIdx < MipCount; MipIdx++)
	{
		uint32_t MipSize = (ImageExtent.width >> MipIdx) * (ImageExtent.height >> MipIdx) * ByteSize;

		ReadbackBuffer[MipIdx].SetDeviceInfo(GfxCache->GetLogicalDevice(), GfxCache->GetDeviceMemProps());
		ReadbackBuffer[MipIdx].CreateBuffer(MipSize, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		VkBufferImageCopy Region{};
		Region.bufferOffset = 0;
		Region.bufferRowLength = 0;
		Region.bufferImageHeight = 0;

		Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.imageSubresource.mipLevel = MipIdx;
		Region.imageSubresource.baseArrayLayer = 0;
		Region.imageSubresource.layerCount = 1;

		Region.imageOffset = { 0, 0, 0 };
		Region.imageExtent = { ImageExtent.width >> MipIdx, ImageExtent.height >> MipIdx, 1 };

		ReadbackBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, MipIdx);
		vkCmdCopyImageToBuffer(ReadbackCmd, ImageSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ReadbackBuffer[MipIdx].GetBuffer(), 1, &Region);
		ReadbackBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, MipIdx);
	}

	GfxCache->EndOneTimeCmd(ReadbackCmd);

	for (uint32_t MipIdx = 0; MipIdx < MipCount; MipIdx++)
	{
		std::vector<uint8_t> MipData = ReadbackBuffer[MipIdx].ReadbackData();
		NewData.insert(NewData.end(), MipData.begin(), MipData.end());
		ReadbackBuffer[MipIdx].Release();
	}

	return NewData;
}

void UHTexture2D::Export(std::filesystem::path InTexturePath)
{
	// open UHTexture file
	std::ofstream FileOut(InTexturePath.string() + GTextureAssetExtension, std::ios::out | std::ios::binary);

	// write version and type
	TextureVersion = static_cast<UHTextureVersion>(TextureVersionMax - 1);
	FileOut.write(reinterpret_cast<const char*>(&TextureVersion), sizeof(TextureVersion));
	FileOut.write(reinterpret_cast<const char*>(&TextureType), sizeof(TextureType));

	// write texture name, source path
	UHUtilities::WriteStringData(FileOut, Name);
	UHUtilities::WriteStringData(FileOut, SourcePath);
	UHUtilities::WriteStringData(FileOut, RawSourcePath);

	// write image extent
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.height), sizeof(ImageExtent.height));

	// write texture data
	UHUtilities::WriteVectorData(FileOut, TextureData);

	// write texture settings
	FileOut.write(reinterpret_cast<char*>(&TextureSettings), sizeof(TextureSettings));

	FileOut.close();
}
#endif

void UHTexture2D::SetTextureData(std::vector<uint8_t> InData)
{
	TextureData = InData;
}

std::vector<uint8_t>& UHTexture2D::GetTextureData()
{
	return TextureData;
}

void UHTexture2D::UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder)
{
	if (bHasUploadedToGPU)
	{
		return;
	}

	// copy data to staging buffer first
	RawStageBuffers.resize(GetMipMapCount());
	const UHTextureFormatData TextureFormatData = GTextureFormatData[ImageFormat];
	uint64_t MipStartIndex = 0;

	for (uint32_t MipIdx = 0; MipIdx < GetMipMapCount(); MipIdx++)
	{
		uint64_t MipSize = (ImageExtent.width >> MipIdx) * (ImageExtent.height >> MipIdx) * TextureFormatData.ByteSize / TextureFormatData.BlockSize;
		if (TextureSettings.bIsCompressed)
		{
			MipSize = std::max(MipSize, static_cast<uint64_t>(TextureFormatData.ByteSize));
		}

		if (MipStartIndex >= TextureData.size())
		{
			continue;
		}

		RawStageBuffers[MipIdx].SetDeviceInfo(InGfx->GetLogicalDevice(), InGfx->GetDeviceMemProps());
		RawStageBuffers[MipIdx].CreateBuffer(MipSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		RawStageBuffers[MipIdx].UploadAllData(TextureData.data() + MipStartIndex);
		MipStartIndex += MipSize;
	}

	for (uint32_t Mdx = 0; Mdx < GetMipMapCount(); Mdx++)
	{
		// copy buffer to image
		VkBuffer SrcBuffer = RawStageBuffers[Mdx].GetBuffer();
		VkImage DstImage = GetImage();
		VkExtent2D Extent = GetExtent();

		if (SrcBuffer == nullptr)
		{
			continue;
		}

		// set up copying region
		VkBufferImageCopy Region{};
		Region.bufferOffset = 0;
		Region.bufferRowLength = 0;
		Region.bufferImageHeight = 0;

		Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.imageSubresource.mipLevel = Mdx;
		Region.imageSubresource.baseArrayLayer = 0;
		Region.imageSubresource.layerCount = 1;

		Region.imageOffset = { 0, 0, 0 };
		Region.imageExtent = { Extent.width >> Mdx, Extent.height >> Mdx, 1 };

		// transition to dst before copy
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx);
		vkCmdCopyBufferToImage(InCmd, SrcBuffer, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
	}

	bHasUploadedToGPU = true;
}

void UHTexture2D::GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder)
{
	// generate mip maps for this texture, should be called in editor only
	if (bIsMipMapGenerated || !TextureSettings.bUseMipmap)
	{
		return;
	}

	VkImageViewCreateInfo TexInfo = GetImageViewInfo();
	int32_t MipWidth = GetExtent().width;
	int32_t MipHeight = GetExtent().height;

	for (uint32_t Mdx = 1; Mdx < TexInfo.subresourceRange.levelCount; Mdx++)
	{
		// transition mip M-1 to SRC BIT, and M to DST BIT, note mip M is still layout undefined at this point
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Mdx - 1);
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx);

		// blit to proper mip slices
		VkExtent2D SrcExtent;
		VkExtent2D DstExtent;
		SrcExtent.width = MipWidth;
		SrcExtent.height = MipHeight;
		DstExtent.width = MipWidth >> 1;
		DstExtent.height = MipHeight >> 1;
		InRenderBuilder.Blit(this, this, SrcExtent, DstExtent, Mdx - 1, Mdx);
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);

		// for next mip
		MipWidth = MipWidth >> 1;
		MipHeight = MipHeight >> 1;
	}

	// transition all texture to shader read after generating mip maps
	for (uint32_t Mdx = 0; Mdx < TexInfo.subresourceRange.levelCount; Mdx++)
	{
		// the last mip is DST bit, differeniate here
		if (Mdx != TexInfo.subresourceRange.levelCount - 1)
		{
			InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
		}
	}

	bIsMipMapGenerated = true;
}

bool UHTexture2D::CreateTexture(bool bFromSharedMemory)
{
	// texture also needs SRC/DST bits for copying/blit operation
	UHTextureInfo Info(VK_IMAGE_TYPE_2D
		, VK_IMAGE_VIEW_TYPE_2D, ImageFormat, ImageExtent
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	Info.ReboundOffset = MemoryOffset;
	bSharedMemory = bFromSharedMemory;

	if (ImageFormat == UH_FORMAT_NONE)
	{
		ImageFormat = (TextureSettings.bIsLinear) ? UH_FORMAT_RGBA8_UNORM : UH_FORMAT_RGBA8_SRGB;
	}

	if (TextureSettings.bIsCompressed)
	{
		switch (TextureSettings.CompressionSetting)
		{
		case BC1:
			Info.Format = (TextureSettings.bIsLinear) ? UH_FORMAT_BC1_UNORM : UH_FORMAT_BC1_SRGB;
			break;

		case BC3:
			Info.Format = (TextureSettings.bIsLinear) ? UH_FORMAT_BC3_UNORM : UH_FORMAT_BC3_SRGB;
			break;

		case BC4:
			Info.Format = UH_FORMAT_BC4;
			break;

		case BC5:
			Info.Format = UH_FORMAT_BC5;
			break;

		case BC6H:
			Info.Format = UH_FORMAT_BC6H;
			break;

		default:
			break;
		}
	}
	else if (TextureSettings.bIsHDR)
	{
		Info.Format = UH_FORMAT_RGBA16F;
	}

	return Create(Info, (bFromSharedMemory) ? GfxCache->GetImageSharedMemory() : nullptr);
}