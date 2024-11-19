#pragma once
#include "../Engine/RenderResource.h"

class UHGraphic;

struct UHSamplerInfo
{
	UHSamplerInfo(VkFilter InFilter, VkSamplerAddressMode InAddressModeU, VkSamplerAddressMode InAddressModeV
		, VkSamplerAddressMode InAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT
		, float InMaxAnisotropy = 16.0f)
		: FilterMode(InFilter)
		, AddressModeU(InAddressModeU)
		, AddressModeV(InAddressModeV)
		, AddressModeW(InAddressModeW)
		, MaxAnisotropy(InMaxAnisotropy)
		, CompareOp(VK_COMPARE_OP_ALWAYS)
		, MipBias(-1)
	{
	}

	bool operator==(const UHSamplerInfo& InInfo)
	{
		return InInfo.FilterMode == FilterMode
			&& InInfo.AddressModeU == AddressModeU
			&& InInfo.AddressModeV == AddressModeV
			&& InInfo.AddressModeW == AddressModeW
			&& InInfo.MaxAnisotropy == MaxAnisotropy
			&& InInfo.CompareOp == CompareOp
			&& InInfo.MipBias == MipBias;
	}

#if WITH_EDITOR
	std::string AddressModeToString(VkSamplerAddressMode InMode) const
	{
		if (InMode == VK_SAMPLER_ADDRESS_MODE_REPEAT)
		{
			return "_Wrap";
		}
		else if (InMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
		{
			return "_Clamp";
		}
		else if (InMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
		{
			return "_ClampBorder";
		}
		else if (InMode == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT)
		{
			return "_MirroredWrap";
		}
		else if (InMode == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE)
		{
			return "_MirroredClamp";
		}

		return "";
	}

	std::string GetDebugName() const
	{
		std::string Name;
		if (FilterMode == VK_FILTER_NEAREST)
		{
			Name += "_Nearest";
		}
		else if (FilterMode == VK_FILTER_LINEAR)
		{
			Name += "_Linear";
		}

		Name += AddressModeToString(AddressModeU) + "_U";
		Name += AddressModeToString(AddressModeV) + "_V";
		Name += AddressModeToString(AddressModeW) + "_W";

		if (MaxAnisotropy > 1)
		{
			Name += "_Aniso" + std::to_string(MaxAnisotropy);
		}

		return Name;
	}
#endif

	VkFilter FilterMode;
	VkSamplerAddressMode AddressModeU;
	VkSamplerAddressMode AddressModeV;
	VkSamplerAddressMode AddressModeW;
	float MaxAnisotropy;
	VkCompareOp CompareOp;
	float MipBias;
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