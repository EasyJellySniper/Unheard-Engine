#pragma once
#include "Types.h"
#include "RenderBuffer.h"
#include <memory>
#include <string>
#include <array>

/********************************************* mesh layouts define of UH engine *********************************************/

// a struct to collect all vertex attributes
// doesn't mean this is going to be final structure used in GPU (E.g. In unlit model, I could strip out Normal/Tangent)
// zero-based uv naming
struct UHMeshData
{
	XMFLOAT3 Position;
	XMFLOAT2 UV0;		// from eTextureDiffuse, will be used as base UV
	XMFLOAT3 Normal;
	XMFLOAT4 Tangent;
};

// using UHMeshData as default lit layout
using UHDefaultLitMeshLayout = UHMeshData;

template <typename T>
inline VkPipelineVertexInputStateCreateInfo GetVertexInputInfo(VkVertexInputBindingDescription& OutBindingDesc, std::vector<VkVertexInputAttributeDescription>& OutAttributeDesc)
{
	VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
	OutBindingDesc = VkVertexInputBindingDescription{};

	// get binding desc based on layout type
	if (std::is_same<T, UHDefaultLitMeshLayout>::value)
	{
		OutBindingDesc.binding = 0;
		OutBindingDesc.stride = sizeof(T);
		OutBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		OutAttributeDesc.resize(4);
		OutAttributeDesc[0].binding = 0;
		OutAttributeDesc[0].location = 0;
		OutAttributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		OutAttributeDesc[0].offset = offsetof(UHDefaultLitMeshLayout, Position);
		
		OutAttributeDesc[1].binding = 0;
		OutAttributeDesc[1].location = 1;
		OutAttributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
		OutAttributeDesc[1].offset = offsetof(UHDefaultLitMeshLayout, UV0);
		
		OutAttributeDesc[2].binding = 0;
		OutAttributeDesc[2].location = 2;
		OutAttributeDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		OutAttributeDesc[2].offset = offsetof(UHDefaultLitMeshLayout, Normal);
		
		OutAttributeDesc[3].binding = 0;
		OutAttributeDesc[3].location = 3;
		OutAttributeDesc[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		OutAttributeDesc[3].offset = offsetof(UHDefaultLitMeshLayout, Tangent);

		VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputInfo.vertexBindingDescriptionCount = 1;
		VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(OutAttributeDesc.size());
		VertexInputInfo.pVertexBindingDescriptions = &OutBindingDesc;
		VertexInputInfo.pVertexAttributeDescriptions = OutAttributeDesc.data();
	}

	return VertexInputInfo;
}