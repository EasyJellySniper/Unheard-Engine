#include "TextureCube.h"
#include "../Renderer/RenderBuilder.h"

UHTextureCube::UHTextureCube()
	: UHTextureCube("", VkExtent2D(), VK_FORMAT_UNDEFINED)
{

}

UHTextureCube::UHTextureCube(std::string InName, VkExtent2D InExtent, VkFormat InFormat)
	: UHTexture(InName, InExtent, InFormat, UHTextureSettings())
	, bIsCubeBuilt(false)
{
	TextureType = TextureCube;
}

// actually builds cubemap and upload to gpu
void UHTextureCube::Build(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder)
{
	if (bIsCubeBuilt)
	{
		return;
	}

	// simply copy all slices into cube map
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		// if texture slices isn't built yet, build it
		Slices[Idx]->UploadToGPU(InGfx, InCmd, InRenderBuilder);
		Slices[Idx]->GenerateMipMaps(InGfx, InCmd, InRenderBuilder);

		// transition and copy, all mip maps need to be copied
		for (uint32_t Mdx = 0; Mdx < Slices[Idx]->GetMipMapCount(); Mdx++)
		{
			InRenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Mdx);
			InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Mdx, Idx);
			InRenderBuilder.CopyTexture(Slices[Idx], this, Mdx, Idx);
			InRenderBuilder.ResourceBarrier(this, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx, Idx);
			InRenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx);
		}
	}

	bIsCubeBuilt = true;
}

bool UHTextureCube::CreateCube(std::vector<UHTexture2D*> InSlices)
{
	Slices = InSlices;

	// texture also needs SRC/DST bits for copying/blit operation
	// setup view type as Cube, the data is actually a 2D texture array
	UHTextureInfo Info(VK_IMAGE_TYPE_2D
		, VK_IMAGE_VIEW_TYPE_CUBE, GetFormat(), GetExtent()
		, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false, true);

	return Create(Info, GfxCache->GetImageSharedMemory());
}