#pragma once
#include "../Engine/RenderResource.h"

class UHGraphic;

struct UHSamplerInfo
{
	UHSamplerInfo(VkFilter InFilter, VkSamplerAddressMode InAddressModeU, VkSamplerAddressMode InAddressModeV
		, VkSamplerAddressMode InAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT
		, float InMaxLod = 0.0f
		, float InMaxAnisotropy = 8.0f)
		: FilterMode(InFilter)
		, AddressModeU(InAddressModeU)
		, AddressModeV(InAddressModeV)
		, AddressModeW(InAddressModeW)
		, MaxLod(InMaxLod)
		, MaxAnisotropy(InMaxAnisotropy)
		, CompareOp(VK_COMPARE_OP_ALWAYS)
	{
	}

	bool operator==(const UHSamplerInfo& InInfo)
	{
		return InInfo.FilterMode == FilterMode
			&& InInfo.AddressModeU == AddressModeU
			&& InInfo.AddressModeV == AddressModeV
			&& InInfo.AddressModeW == AddressModeW
			&& InInfo.MaxAnisotropy == MaxAnisotropy
			&& InInfo.MaxLod == MaxLod
			&& InInfo.CompareOp == CompareOp;
	}

	VkFilter FilterMode;
	VkSamplerAddressMode AddressModeU;
	VkSamplerAddressMode AddressModeV;
	VkSamplerAddressMode AddressModeW;
	float MaxLod;
	float MaxAnisotropy;
	VkCompareOp CompareOp;
};

class UHSampler : public UHRenderResource
{
public:
	UHSampler(UHSamplerInfo InInfo);
	void Release();

	VkSampler GetSampler() const;
	UHSamplerInfo GetSamplerInfo() const;
	bool operator==(const UHSampler& InSampler);

private:
	bool Create();
	VkSampler TextureSampler;

	UHSamplerInfo SamplerInfo;

	friend UHGraphic;
};