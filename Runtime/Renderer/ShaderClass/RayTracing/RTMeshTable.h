#pragma once
#include "../ShaderClass.h"

class UHRTVertexTable : public UHShaderClass
{
public:
	UHRTVertexTable() {}
	UHRTVertexTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfMeshes)
		: UHShaderClass(InGfx, Name, typeid(UHRTVertexTable))
	{
		// simply create layout with number of meshes
		AddLayoutBinding(NumOfMeshes, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateDescriptor();
	}
};

class UHRTIndicesTable : public UHShaderClass
{
public:
	UHRTIndicesTable() {}
	UHRTIndicesTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfMeshes)
		: UHShaderClass(InGfx, Name, typeid(UHRTIndicesTable))
	{
		// simply create layout with number of meshes
		AddLayoutBinding(NumOfMeshes, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateDescriptor();
	}
};

class UHRTIndicesTypeTable : public UHShaderClass
{
public:
	UHRTIndicesTypeTable() {}
	UHRTIndicesTypeTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHRTIndicesTypeTable))
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateDescriptor();
	}
};