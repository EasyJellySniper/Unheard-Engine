#include "RenderTexture.h"
#include "../Classes/Utility.h"
#include "../../UnheardEngine.h"
#include "../Engine/Graphic.h"
#include "../Renderer/RenderBuilder.h"

UHRenderTexture::UHRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, bool bReadWrite, bool bInUseMipmap)
	: UHTexture(InName, InExtent, InFormat, UHTextureSettings())
	, bIsReadWrite(bReadWrite)
	, bUseMipmap(bInUseMipmap)
{
	bCreatePerMipImageView = true;
}

// Similar as the implementation of UHTexture2D
void UHRenderTexture::GenerateMipMaps(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder)
{
	if (!TextureSettings.bUseMipmap || !GTextureFormatData[UH_ENUM_VALUE(ImageFormat)].bCanGenerateMipmap)
	{
		return;
	}

	VkImageViewCreateInfo TexInfo = GetImageViewInfo();
	int32_t MipWidth = GetExtent().width;
	int32_t MipHeight = GetExtent().height;

	for (uint32_t Mdx = 1; Mdx < TexInfo.subresourceRange.levelCount; Mdx++)
	{
		// transition mip M-1 to SRC BIT, and M to DST BIT, note mip M is still layout undefined at this point
		InRenderBuilder.PushResourceBarrier(UHImageBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Mdx - 1));
		InRenderBuilder.PushResourceBarrier(UHImageBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx));
		InRenderBuilder.FlushResourceBarrier();

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
		InRenderBuilder.PushResourceBarrier(UHImageBarrier(this, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
	}
	InRenderBuilder.FlushResourceBarrier();
}

#if WITH_EDITOR
std::vector<uint8_t> UHRenderTexture::ReadbackTextureData()
{
	// almost the same implement as texture 2D version
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
#endif

// create image based on format and extent info
bool UHRenderTexture::CreateRT()
{
	VkImageUsageFlags Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (bIsReadWrite)
	{
		Usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	UHTextureInfo Info(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, ImageFormat, ImageExtent, Usage, true);
	TextureSettings.bUseMipmap = bUseMipmap;

	// UHE doesn't use shared memory for RTs at the momment, since they could resize
	return Create(Info, nullptr);
}