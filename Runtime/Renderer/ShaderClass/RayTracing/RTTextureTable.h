#pragma once
#include "../ShaderClass.h"

// Texture table used for ray tracing
// It's not the same as TextureSamplerTable.h!
// The texture defined for this table would be used as render target.
// That's why they need to be seperated from TextureSamplerTable.
const uint32_t GRTTextureTableSize = 5;

class UHRTTextureTable : public UHShaderClass
{
public:
	UHRTTextureTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHRTTextureTable), nullptr)
	{
		AddLayoutBinding(GRTTextureTableSize, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0);
		CreateDescriptor();
	}

	virtual void OnCompile() override {}
};