#include "TextureSamplerTable.h"

// UHTextureTable
uint32_t UHTextureTable::MaxNumTexes = 1024;

UHTextureTable::UHTextureTable(UHGraphic* InGfx, std::string Name, uint32_t NumTexes)
	: UHShaderClass(InGfx, Name, typeid(UHTextureTable), nullptr)
{
	AddLayoutBinding(NumTexes, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0);
	CreateLayoutAndDescriptor();
	this->NumTexes = NumTexes;
}

uint32_t UHTextureTable::GetNumTexes() const
{
	return NumTexes;
}

// UHSamplerTable
UHSamplerTable::UHSamplerTable(UHGraphic* InGfx, std::string Name, uint32_t NumSamplers)
	: UHShaderClass(InGfx, Name, typeid(UHSamplerTable), nullptr)
{
	AddLayoutBinding(NumSamplers, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER, 0);
	CreateLayoutAndDescriptor();
	this->NumSamplers = NumSamplers;
}

uint32_t UHSamplerTable::GetNumSamplers() const
{
	return NumSamplers;
}