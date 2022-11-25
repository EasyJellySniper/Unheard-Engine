#pragma once
#include "../../UnheardEngine.h"
#include "../Engine/RenderResource.h"
#include "RenderBuffer.h"

enum UHGPUQueryState
{
	Idle = 0,
	Requested
};

// UH GPU Query object
class UHGPUQuery : public UHRenderResource
{
public:
	UHGPUQuery();
	void Release();
	void CreateQueryPool(uint32_t QueryCount, VkQueryType QueryType);

	void BeginTimeStamp(VkCommandBuffer InBuffer);
	void EndTimeStamp(VkCommandBuffer InBuffer);
	float GetTimeStamp(VkCommandBuffer InBuffer);

private:
	uint32_t QueryCount;
	UHGPUQueryState State;
	VkQueryPool QueryPool;
	float PreviousValidTimeStamp;
};