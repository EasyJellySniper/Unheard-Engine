#pragma once
#include "../ShaderClass.h"

// texture table class for ray tracing, I just want the descriptor set
class UHRTTextureTable : public UHShaderClass
{
public:
	UHRTTextureTable() {}
	UHRTTextureTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfTextures)
		: UHShaderClass(InGfx, Name, typeid(UHRTTextureTable), nullptr)
	{
		AddLayoutBinding(NumOfTextures, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0);
		CreateDescriptor();
	}
};

// sampler table
class UHRTSamplerTable : public UHShaderClass
{
public:
	UHRTSamplerTable() {}
	UHRTSamplerTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfSamplers)
		: UHShaderClass(InGfx, Name, typeid(UHRTSamplerTable), nullptr)
	{
		AddLayoutBinding(NumOfSamplers, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER, 0);
		CreateDescriptor();
	}
};