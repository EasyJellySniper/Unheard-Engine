#include "RenderResource.h"

UHRenderResource::UHRenderResource()
	: GfxCache(nullptr)
	, LogicalDevice(VK_NULL_HANDLE)
	, DeviceMemoryProperties(VkPhysicalDeviceMemoryProperties())
	, VulkanInstance(VK_NULL_HANDLE)
{

}

void UHRenderResource::SetGfxCache(UHGraphic* InGfx)
{
	GfxCache = InGfx;
}

void UHRenderResource::SetVulkanInstance(VkInstance InInstance)
{
	VulkanInstance = InInstance;
}

// cache device info
void UHRenderResource::SetDeviceInfo(VkDevice InDevice, VkPhysicalDeviceMemoryProperties InMemProps)
{
	LogicalDevice = InDevice;
	DeviceMemoryProperties = InMemProps;
}