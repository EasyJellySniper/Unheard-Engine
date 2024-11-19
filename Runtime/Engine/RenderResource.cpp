#include "RenderResource.h"
#include "Runtime/Classes/Types.h"
#include "Runtime/Engine/Graphic.h"

UHRenderResource::UHRenderResource()
	: GfxCache(nullptr)
	, LogicalDevice(nullptr)
	, DeviceMemoryProperties(VkPhysicalDeviceMemoryProperties())
	, VulkanInstance(nullptr)
	, IndexInPool(UHINDEXNONE)
	, ReferenceCount(0)
{

}

void UHRenderResource::SetGfxCache(UHGraphic* InGfx)
{
	GfxCache = InGfx;
	LogicalDevice = GfxCache->GetLogicalDevice();
	DeviceMemoryProperties = GfxCache->GetDeviceMemProps();
	VulkanInstance = GfxCache->GetInstance();
}

void UHRenderResource::SetIndexInPool(const int32_t InIndex)
{
	IndexInPool = InIndex;
}

void UHRenderResource::IncreaseRefCount()
{
	ReferenceCount++;
}

void UHRenderResource::DecreaseRefCount()
{
	ReferenceCount--;
	assert(ReferenceCount >= 0);
}

int32_t UHRenderResource::GetIndexInPool() const
{
	return IndexInPool;
}

int32_t UHRenderResource::GetRefCount() const
{
	return ReferenceCount;
}