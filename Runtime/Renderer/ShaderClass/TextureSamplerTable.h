#pragma once
#include "ShaderClass.h"

// texture and sampler table for bindless rendering
// @TODO: What if number of textures are large than this number?
const uint32_t GTextureTableSize = 1024;

// texture table
class UHTextureTable : public UHShaderClass
{
public:
	UHTextureTable() {}
	UHTextureTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHTextureTable), nullptr)
	{
		AddLayoutBinding(GTextureTableSize, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0);
		CreateDescriptor();
	}
};

// sampler table
class UHSamplerTable : public UHShaderClass
{
public:
	UHSamplerTable() {}
	UHSamplerTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfSamplers)
		: UHShaderClass(InGfx, Name, typeid(UHSamplerTable), nullptr)
	{
		AddLayoutBinding(NumOfSamplers, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER, 0);
		CreateDescriptor();
	}
};