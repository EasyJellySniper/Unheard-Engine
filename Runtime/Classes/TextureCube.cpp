#include "TextureCube.h"
#include "../Renderer/RenderBuilder.h"
#include "AssetPath.h"
#include "Utility.h"

UHTextureCube::UHTextureCube()
	: UHTextureCube("", VkExtent2D(), UH_FORMAT_NONE, UHTextureSettings())
{

}

UHTextureCube::UHTextureCube(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings)
	: UHTexture(InName, InExtent, InFormat, InSettings)
	, bIsCubeBuilt(false)
{
	TextureType = TextureCube;
}

void UHTextureCube::ReleaseCPUData()
{
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		SliceData[Idx].clear();
		for (UHRenderBuffer<uint8_t>& Buffer : RawStageBuffers[Idx])
		{
			Buffer.Release();
		}
	}
}

void UHTextureCube::SetCubeData(std::vector<uint8_t> InData, int32_t Slice)
{
	SliceData[Slice] = std::move(InData);
}

std::vector<uint8_t> UHTextureCube::GetCubeData(int32_t Slice) const
{
	return SliceData[Slice];
}

bool UHTextureCube::IsBuilt() const
{
	return bIsCubeBuilt;
}

bool UHTextureCube::Import(std::filesystem::path InCubePath)
{
	if (InCubePath.extension() != GCubemapAssetExtension)
	{
		return false;
	}

	std::ifstream FileIn(InCubePath.string().c_str(), std::ios::in | std::ios::binary);
	if (!FileIn.is_open())
	{
		UHE_LOG(L"Failed to Load UHCubemap " + InCubePath.wstring() + L"!\n");
		return false;
	}

	UHObject::OnLoad(FileIn);
	// read type and source path
	FileIn.read(reinterpret_cast<char*>(&TextureType), sizeof(TextureType));
	UHUtilities::ReadStringData(FileIn, SourcePath);

	// read extent and format
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.height), sizeof(ImageExtent.height));
	FileIn.read(reinterpret_cast<char*>(&ImageFormat), sizeof(ImageFormat));

	// read slice data
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		UHUtilities::ReadVectorData(FileIn, SliceData[Idx]);
	}

	// read texture settings
	FileIn.read(reinterpret_cast<char*>(&TextureSettings), sizeof(TextureSettings));

	FileIn.close();

	return true;
}

#if WITH_EDITOR
void UHTextureCube::SetSlices(std::vector<UHTexture2D*> InSlices)
{
	Slices = InSlices;
}

void UHTextureCube::Recreate(UHTextureFormat NewFormat)
{
	GfxCache->WaitGPU();
	Release();
	ImageFormat = NewFormat;

	if (Slices.size() > 0)
	{
		CreateCube(Slices);
	}
	else
	{
		CreateCube();
	}

	bIsCubeBuilt = false;
}

void UHTextureCube::Export(std::filesystem::path InCubePath)
{
	// open UHTexture file
	std::ofstream FileOut(InCubePath.string() + GCubemapAssetExtension, std::ios::out | std::ios::binary);

	Version = static_cast<UHTextureVersion>(TextureVersionMax - 1);
	UHObject::OnSave(FileOut);

	// write type and source path
	FileOut.write(reinterpret_cast<const char*>(&TextureType), sizeof(TextureType));
	UHUtilities::WriteStringData(FileOut, SourcePath);

	// write image extent
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.height), sizeof(ImageExtent.height));
	FileOut.write(reinterpret_cast<const char*>(&ImageFormat), sizeof(ImageFormat));

	// write texture data
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		UHUtilities::WriteVectorData(FileOut, SliceData[Idx]);
	}

	// write texture settings
	FileOut.write(reinterpret_cast<char*>(&TextureSettings), sizeof(TextureSettings));

	FileOut.close();
}

void UHTextureCube::SetSourcePath(std::filesystem::path InPath)
{
	SourcePath = InPath.string();
}

size_t UHTextureCube::GetDataSize() const
{
	size_t TotalSize = 0;
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		TotalSize += SliceData[Idx].size();
	}

	return TotalSize;
}

#endif

// actually builds cubemap and upload to gpu
void UHTextureCube::Build(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder)
{
	if (bIsCubeBuilt)
	{
		return;
	}

	MipMapCount = TextureSettings.bUseMipmap ? static_cast<uint32_t>(std::floor(std::log2((std::min)(ImageExtent.width, ImageExtent.height)))) + 1 : 1;
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		if (Slices.size() > 0 && Slices[Idx] != nullptr)
		{
			// simply copy all slices into cube map
			// if texture slices isn't built yet, build it
			Slices[Idx]->UploadToGPU(InGfx, InRenderBuilder);
			Slices[Idx]->GenerateMipMaps(InGfx, InRenderBuilder);

			// transition and copy, all mip maps need to be copied
			for (uint32_t Mdx = 0; Mdx < MipMapCount; Mdx++)
			{
				InRenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Mdx);
				InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx, Idx);
				InRenderBuilder.CopyTexture(Slices[Idx], this, Mdx, Idx);
				InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx, Idx);
				InRenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
			}

			// cache slice data
			SliceData[Idx] = Slices[Idx]->GetTextureData();
		}
		else if (SliceData[Idx].size() > 0)
		{
			// if slices are not there, upload from raw data directly, mipdata is also contained
			UploadSlice(InGfx, InRenderBuilder, Idx, MipMapCount);
		}
		else
		{
			// no data (could be dummy fallback cube), transition to shader read
			for (uint32_t Mdx = 0; Mdx < MipMapCount; Mdx++)
			{
				InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx, Idx);
			}
		}
	}

	bIsCubeBuilt = true;
}

void UHTextureCube::UploadSlice(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder, const int32_t SliceIndex, const uint32_t MipMapCount)
{
	// copy data to staging buffer first
	RawStageBuffers[SliceIndex].resize(MipMapCount);
	const UHTextureFormatData TextureFormatData = GTextureFormatData[ImageFormat];
	uint64_t MipStartIndex = 0;

	for (uint32_t MipIdx = 0; MipIdx < MipMapCount; MipIdx++)
	{
		uint64_t MipSize = (ImageExtent.width >> MipIdx) * (ImageExtent.height >> MipIdx) * TextureFormatData.ByteSize / TextureFormatData.BlockSize;
		if (TextureSettings.bIsCompressed)
		{
			MipSize = std::max(MipSize, static_cast<uint64_t>(TextureFormatData.ByteSize));
		}

		if (MipStartIndex >= SliceData[SliceIndex].size())
		{
			continue;
		}

		RawStageBuffers[SliceIndex][MipIdx].SetDeviceInfo(InGfx->GetLogicalDevice(), InGfx->GetDeviceMemProps());
		RawStageBuffers[SliceIndex][MipIdx].CreateBuffer(MipSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		RawStageBuffers[SliceIndex][MipIdx].UploadAllData(SliceData[SliceIndex].data() + MipStartIndex);
		MipStartIndex += MipSize;
	}

	for (uint32_t Mdx = 0; Mdx < MipMapCount; Mdx++)
	{
		// copy buffer to image
		VkBuffer SrcBuffer = RawStageBuffers[SliceIndex][Mdx].GetBuffer();
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
		Region.imageSubresource.baseArrayLayer = SliceIndex;
		Region.imageSubresource.layerCount = 1;

		Region.imageOffset = { 0, 0, 0 };
		Region.imageExtent = { Extent.width >> Mdx, Extent.height >> Mdx, 1 };

		// transition to dst before copy
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx, SliceIndex);
		vkCmdCopyBufferToImage(InRenderBuilder.GetCmdList(), SrcBuffer, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);
		InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx, SliceIndex);
	}
}

bool UHTextureCube::CreateCube(std::vector<UHTexture2D*> InSlices)
{
	Slices = InSlices;

	// texture also needs SRC/DST bits for copying/blit operation
	// setup view type as Cube, the data is actually a 2D texture array
	UHTextureInfo Info(VK_IMAGE_TYPE_2D
		, VK_IMAGE_VIEW_TYPE_CUBE, GetFormat(), GetExtent()
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);

	return Create(Info, GfxCache->GetImageSharedMemory());
}

bool UHTextureCube::CreateCube()
{
	UHTextureInfo Info(VK_IMAGE_TYPE_2D
		, VK_IMAGE_VIEW_TYPE_CUBE, GetFormat(), GetExtent()
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);

	return Create(Info, GfxCache->GetImageSharedMemory());
}