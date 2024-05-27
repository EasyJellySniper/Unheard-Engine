#pragma once
#include "../../UnheardEngine.h"
#include "../Engine/RenderResource.h"
#include "RenderBuffer.h"

enum class UHGPUQueryState
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

	VkQueryPool GetQueryPool() const;
	uint32_t GetQueryCount() const;

private:
	uint32_t QueryCount;
	UHGPUQueryState State;
	VkQueryPool QueryPool;
	float PreviousValidTimeStamp;
};

// timestamp query scope version, this simply kicks off timer in ctor and finishs in dtor
// can be used with macro to maintain a clean code
class UHGPUTimeQueryScope
{
public:
	UHGPUTimeQueryScope(VkCommandBuffer InCmd, UHGPUQuery* InQuery);
	~UHGPUTimeQueryScope();

private:
	UHGPUQuery* GPUTimeQuery;
	VkCommandBuffer Cmd;
};