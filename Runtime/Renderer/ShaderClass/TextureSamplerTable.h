#pragma once
#include "ShaderClass.h"

// texture and sampler table for bindless rendering
// @TODO: What if number of textures/samplers are large than this number?
const uint32_t GTextureTableSize = 1024;
const uint32_t GSamplerTableSize = 256;

// texture table
class UHTextureTable : public UHShaderClass
{
public:
	UHTextureTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHTextureTable), nullptr)
	{
		AddLayoutBinding(GTextureTableSize, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0);
		CreateDescriptor();
	}

	virtual void OnCompile() override {}
};

// sampler table
class UHSamplerTable : public UHShaderClass
{
public:
	UHSamplerTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHSamplerTable), nullptr)
	{
		AddLayoutBinding(GSamplerTableSize, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER, 0);
		CreateDescriptor();
	}

	virtual void OnCompile() override {}
};