#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "../Engine/RenderResource.h"

class UHGraphic;

// UH GPU memory class for mananing VkDeviceMemory
// a good usage of VkDeviceMemory should be creating a large one and share use it when possible
// and this hasn't considered reallocation yet! it's used during lifetime for now
class UHGPUMemory : public UHRenderResource
{
public:
	UHGPUMemory();
	void Release();

	void AllocateMemory(uint64_t InBudget, uint32_t MemTypeIndex);
	uint64_t BindMemory(uint64_t InSize, VkBuffer InBuffer);
	uint64_t BindMemory(uint64_t InSize, VkImage InImage, uint64_t ReboundOffset = ~0);
	VkDeviceMemory GetMemory() const;

private:
	uint64_t MemoryBudgetByte;
	VkDeviceMemory BufferMemory;

	// start from 0, if an object is bound, this will increase
	uint64_t CurrentOffset;
};