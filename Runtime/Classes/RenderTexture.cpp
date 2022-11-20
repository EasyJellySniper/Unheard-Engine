#include "RenderTexture.h"
#include "../Classes/Utility.h"
#include "../../UnheardEngine.h"

UHRenderTexture::UHRenderTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, bool bIsLinear, bool bReadWrite, bool bShadowRT)
	: UHTexture(InName, InExtent, InFormat, bIsLinear)
	, bIsReadWrite(bReadWrite)
	, bIsShadowRT(bShadowRT)
{

}

// create image based on format and extent info
bool UHRenderTexture::CreateRT()
{
	VkImageUsageFlags Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (bIsReadWrite)
	{
		Usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	UHTextureInfo Info(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, ImageFormat, ImageExtent, Usage, true, false);
	Info.bIsShadowRT = bIsShadowRT;

	return Create(Info);
}