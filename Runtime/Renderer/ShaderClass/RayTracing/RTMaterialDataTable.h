#pragma once
#include "../ShaderClass.h"

class UHRTMaterialDataTable : public UHShaderClass
{
public:
	UHRTMaterialDataTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfMaterials)
		: UHShaderClass(InGfx, Name, typeid(UHRTMaterialDataTable), nullptr)
	{
		// simply create layout with number of meshes
		AddLayoutBinding(NumOfMaterials, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateDescriptor();
	}

	virtual void OnCompile() override {}
};
