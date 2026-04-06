#pragma once
#include "Runtime/Platform/PlatformVulkan.h"
#include "../Classes/Object.h"

class UHGraphic;

// class that handles render resources
// UH resources need to be requested from UHGraphic
class UHRenderResource : public UHObject
{
public:
	UHRenderResource();
	void SetGfxCache(UHGraphic* InGfx);
	void SetIndexInPool(const int32_t InIndex);
	void IncreaseRefCount();
	void DecreaseRefCount();

	int32_t GetIndexInPool() const;
	int32_t GetRefCount() const;

protected:
	uint32_t GetHostMemoryTypeIndex() const;

	UHGraphic* GfxCache;
	VkDevice LogicalDevice;
	int32_t IndexInPool;
	int32_t ReferenceCount;
};