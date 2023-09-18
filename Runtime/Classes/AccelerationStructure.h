#pragma once
#include "../Engine/RenderResource.h"
#include "../Classes/Utility.h"
#include "../Classes/Types.h"
#include "../../UnheardEngine.h"
#include "RenderBuffer.h"

class UHMesh;
class UHMeshRendererComponent;

// a class for managing acceleration structure
class UHAccelerationStructure : public UHRenderResource
{
public:
	UHAccelerationStructure();

	// either call bottomAS or TopAS, only one instance is stored
	void CreaetBottomAS(UHMesh* InMesh, VkCommandBuffer InBuffer);

	// Create top level AS, return number of instance built
	uint32_t CreateTopAS(const std::vector<UHMeshRendererComponent*>& InRenderers, VkCommandBuffer InBuffer);
	void UpdateTopAS(VkCommandBuffer InBuffer, int32_t CurrentFrameRT);

	void Release();
	void ReleaseScratch();

	VkAccelerationStructureKHR GetAS() const;

private:
	VkDeviceAddress GetDeviceAddress(VkBuffer InBuffer);
	VkDeviceAddress GetDeviceAddress(VkAccelerationStructureKHR InAS);

	UniquePtr<UHRenderBuffer<BYTE>> ScratchBuffer;
	UniquePtr<UHRenderBuffer<VkAccelerationStructureInstanceKHR>> ASInstanceBuffer;
	UniquePtr<UHRenderBuffer<BYTE>> AccelerationStructureBuffer;
	VkAccelerationStructureKHR AccelerationStructure;

	// instance KHRs and renderer cache, both should the same length
	std::vector<VkAccelerationStructureInstanceKHR> InstanceKHRs;
	std::vector<UHMeshRendererComponent*> RendererCache;

	// cache the info too
	VkAccelerationStructureGeometryKHR GeometryKHRCache;
	VkAccelerationStructureBuildGeometryInfoKHR GeometryInfoCache;
	VkAccelerationStructureBuildRangeInfoKHR RangeInfoCache;
};