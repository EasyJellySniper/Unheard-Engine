#include "Texture2D.h"
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "AssetPath.h"
#include "../Engine/Graphic.h"
#include "../Renderer/GraphicBuilder.h"

UHTexture2D::UHTexture2D()
	: UHTexture()
{

}

UHTexture2D::UHTexture2D(std::string InName, std::string InSourcePath, VkExtent2D InExtent, VkFormat InFormat, bool bInIsLinear)
	: UHTexture(InName, InExtent, InFormat, bInIsLinear)
{
	SourcePath = InSourcePath;
}

void UHTexture2D::ReleaseCPUTextureData()
{
	StageBuffer.Release();
	TextureData.clear();
}

bool UHTexture2D::Import(std::filesystem::path InTexturePath)
{
	std::ifstream FileIn(InTexturePath.string().c_str(), std::ios::in | std::ios::binary);
	if (!FileIn.is_open())
	{
		UHE_LOG(L"Failed to Load UHTexture " + InTexturePath.wstring() + L"!\n");
		return false;
	}

	// read texture name
	UHUtilities::ReadStringData(FileIn, Name);
	UHUtilities::ReadStringData(FileIn, SourcePath);

	// read extent
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileIn.read(reinterpret_cast<char*>(&ImageExtent.height), sizeof(ImageExtent.height));

	// read texture data
	UHUtilities::ReadVectorData(FileIn, TextureData);

	// read linear flag
	FileIn.read(reinterpret_cast<char*>(&bIsLinear), sizeof(bIsLinear));

	FileIn.close();

	return true;
}

#if WITH_DEBUG
void UHTexture2D::Export(std::filesystem::path InTexturePath)
{
	if (!std::filesystem::exists(InTexturePath))
	{
		std::filesystem::create_directories(InTexturePath);
	}

	// open UHMesh file
	std::ofstream FileOut(InTexturePath.string() + Name + GTextureAssetExtension, std::ios::out | std::ios::binary);

	// write texture name
	UHUtilities::WriteStringData(FileOut, Name);
	UHUtilities::WriteStringData(FileOut, SourcePath);

	// write image extent
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.width), sizeof(ImageExtent.width));
	FileOut.write(reinterpret_cast<const char*>(&ImageExtent.height), sizeof(ImageExtent.height));

	// write texture data
	UHUtilities::WriteVectorData(FileOut, TextureData);

	// write linear flag
	FileOut.write(reinterpret_cast<const char*>(&bIsLinear), sizeof(bIsLinear));

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

void UHTexture2D::UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder)
{
	if (bHasUploadedToGPU)
	{
		return;
	}

	// copy data to staging buffer first
	StageBuffer.SetDeviceInfo(InGfx->GetLogicalDevice(), InGfx->GetDeviceMemProps());
	StageBuffer.CreaetBuffer(static_cast<uint64_t>(TextureData.size()), VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	StageBuffer.UploadAllData(TextureData.data());

	// copy buffer to image
	VkBuffer SrcBuffer = StageBuffer.GetBuffer();
	VkImage DstImage = GetImage();
	VkExtent2D Extent = GetExtent();

	// set up copying region
	VkBufferImageCopy Region{};
	Region.bufferOffset = 0;
	Region.bufferRowLength = 0;
	Region.bufferImageHeight = 0;

	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;

	Region.imageOffset = { 0, 0, 0 };
	Region.imageExtent = { Extent.width, Extent.height, 1 };

	// transition to dst before copy
	InGraphBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdCopyBufferToImage(InCmd, SrcBuffer, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	bHasUploadedToGPU = true;
}

void UHTexture2D::GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder)
{
	if (bIsMipMapGenerated)
	{
		return;
	}

	VkImageViewCreateInfo TexInfo = GetImageViewInfo();
	int32_t MipWidth = GetExtent().width;
	int32_t MipHeight = GetExtent().height;

	for (uint32_t Mdx = 1; Mdx < TexInfo.subresourceRange.levelCount; Mdx++)
	{
		// transition mip M-1 to SRC BIT, and M to DST BIT, note mip M is still layout undefined at this point
		InGraphBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Mdx - 1);
		InGraphBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx);

		// blit to proper mip slices
		VkExtent2D SrcExtent;
		VkExtent2D DstExtent;
		SrcExtent.width = MipWidth;
		SrcExtent.height = MipHeight;
		DstExtent.width = MipWidth >> 1;
		DstExtent.height = MipHeight >> 1;
		InGraphBuilder.Blit(this, this, SrcExtent, DstExtent, Mdx - 1, Mdx);

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
			InGraphBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
		}
		else
		{
			InGraphBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
		}
	}

	bIsMipMapGenerated = true;
}

bool UHTexture2D::CreateTextureFromMemory(uint32_t Width, uint32_t Height, std::vector<uint8_t> InTextureData, bool bIsLinear)
{
	VkExtent2D Extent;
	Extent.width = Width;
	Extent.height = Height;

	TextureData = InTextureData;

	// texture also needs SRC/DST bits for copying/blit operation
	UHTextureInfo Info(VK_IMAGE_TYPE_2D
		, VK_IMAGE_VIEW_TYPE_2D, (bIsLinear) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB, Extent
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false, true);

	return Create(Info);
}