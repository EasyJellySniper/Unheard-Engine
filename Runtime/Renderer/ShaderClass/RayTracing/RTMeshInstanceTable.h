#pragma once
#include "../ShaderClass.h"

class UHRTMeshInstanceTable : public UHShaderClass
{
public:
	UHRTMeshInstanceTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHRTMeshInstanceTable), nullptr)
	{
		// simply create layout with number of instances
		VkShaderStageFlags FlagBits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		if (InGfx->IsMeshShaderSupported())
		{
			FlagBits |= VK_SHADER_STAGE_MESH_BIT_EXT;
		}

		AddLayoutBinding(1, FlagBits, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateLayoutAndDescriptor();
	}

	virtual void OnCompile() override {}
};