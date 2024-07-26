#pragma once
#include "ShaderClass.h"

class UHMeshTable : public UHShaderClass
{
public:
	UHMeshTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfInstances)
		: UHShaderClass(InGfx, Name, typeid(UHMeshTable), nullptr)
	{
		// simply create layout with number of instances
		VkShaderStageFlags FlagBits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		if (InGfx->IsMeshShaderSupported())
		{
			FlagBits |= VK_SHADER_STAGE_MESH_BIT_EXT;
		}

		AddLayoutBinding(NumOfInstances, FlagBits, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateLayoutAndDescriptor();
	}

	virtual void OnCompile() override {}
};