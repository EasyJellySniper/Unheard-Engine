#include "Sampler.h"
#include "../../UnheardEngine.h"
#include "../../Runtime/Engine/Graphic.h"

UHSampler::UHSampler(UHSamplerInfo InInfo)
	: TextureSampler(nullptr)
	, SamplerInfo(InInfo)
{

}

void UHSampler::Release()
{
	vkDestroySampler(LogicalDevice, TextureSampler, nullptr);
}

bool UHSampler::Create()
{
	VkSamplerCreateInfo SamplerCreateInfo{};
	SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	SamplerCreateInfo.magFilter = SamplerInfo.FilterMode;
	SamplerCreateInfo.minFilter = SamplerInfo.FilterMode;
	
	SamplerCreateInfo.addressModeU = SamplerInfo.AddressModeU;
	SamplerCreateInfo.addressModeV = SamplerInfo.AddressModeV;
	SamplerCreateInfo.addressModeW = SamplerInfo.AddressModeW;
	
	SamplerCreateInfo.anisotropyEnable = (SamplerInfo.MaxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
	SamplerCreateInfo.maxAnisotropy = SamplerInfo.MaxAnisotropy;
	SamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	// enable compare op if it's not always
	SamplerCreateInfo.compareEnable = (SamplerInfo.CompareOp != VK_COMPARE_OP_ALWAYS) ? VK_TRUE : VK_FALSE;
	SamplerCreateInfo.compareOp = SamplerInfo.CompareOp;
	SamplerCreateInfo.mipmapMode = (SamplerInfo.FilterMode > VK_FILTER_NEAREST) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;

	SamplerCreateInfo.mipLodBias = SamplerInfo.MipBias;
	SamplerCreateInfo.minLod = 0.0f;
	SamplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;

	if (vkCreateSampler(LogicalDevice, &SamplerCreateInfo, nullptr, &TextureSampler) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create texture sampler!\n");
		return false;
	}

#if WITH_EDITOR
	std::string ObjName  = "Sampler" + SamplerInfo.GetDebugName();
	GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_SAMPLER, (uint64_t)TextureSampler, ObjName);
#endif

	return true;
}

VkSampler UHSampler::GetSampler() const
{
	return TextureSampler;
}

UHSamplerInfo UHSampler::GetSamplerInfo() const
{
	return SamplerInfo;
}

bool UHSampler::operator==(const UHSampler& InSampler)
{
	return InSampler.GetSamplerInfo() == SamplerInfo;
}