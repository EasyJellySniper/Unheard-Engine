#pragma once
#include "../ShaderClass.h"

class UHRTVertexTable : public UHShaderClass
{
public:
	UHRTVertexTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfInstances)
		: UHShaderClass(InGfx, Name, typeid(UHRTVertexTable), nullptr)
	{
		// simply create layout with number of instances
		AddLayoutBinding(NumOfInstances, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateLayoutAndDescriptor();
	}

	virtual void OnCompile() override {}
};

class UHRTIndicesTable : public UHShaderClass
{
public:
	UHRTIndicesTable(UHGraphic* InGfx, std::string Name, uint32_t NumOfInstances)
		: UHShaderClass(InGfx, Name, typeid(UHRTIndicesTable), nullptr)
	{
		// simply create layout with number of instances
		AddLayoutBinding(NumOfInstances, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateLayoutAndDescriptor();
	}

	virtual void OnCompile() override {}
};

class UHRTMeshInstanceTable : public UHShaderClass
{
public:
	UHRTMeshInstanceTable(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHRTMeshInstanceTable), nullptr)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
		CreateLayoutAndDescriptor();
	}

	virtual void OnCompile() override {}
};