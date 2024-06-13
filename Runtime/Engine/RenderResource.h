#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "../Classes/Object.h"

class UHGraphic;

// class that handles render resources
// UH resources need to be requested from UHGraphic
class UHRenderResource : public UHObject
{
public:
	UHRenderResource();
	void SetGfxCache(UHGraphic* InGfx);
	void SetVulkanInstance(VkInstance InInstance);
	void SetDeviceInfo(VkDevice InDevice, VkPhysicalDeviceMemoryProperties InMemProps);
	void SetIndexInPool(const int32_t InIndex);
	void IncreaseRefCount();
	void DecreaseRefCount();

	int32_t GetIndexInPool() const;
	int32_t GetRefCount() const;

protected:
	UHGraphic* GfxCache;
	VkInstance VulkanInstance;
	VkDevice LogicalDevice;
	VkPhysicalDeviceMemoryProperties DeviceMemoryProperties;
	int32_t IndexInPool;
	int32_t ReferenceCount;
};