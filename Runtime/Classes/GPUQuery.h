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
	void ResolveTimeStamp(VkCommandBuffer InBuffer);

	VkQueryPool GetQueryPool() const;
	uint32_t GetQueryCount() const;

#if WITH_EDITOR
	void SetDebugName(const std::string& InName);
	std::string GetDebugName() const;
	float GetLastTimeStamp() const;
#endif

private:
	uint32_t QueryCount;
	UHGPUQueryState State;
	VkQueryPool QueryPool;

#if WITH_EDITOR
	std::string DebugName;
	float PreviousValidTimeStamp;
#endif
};

// timestamp query scope version, this simply kicks off timer in ctor and finishs in dtor
// can be used with macro to maintain a clean code
class UHGPUTimeQueryScope
{
public:
	UHGPUTimeQueryScope(VkCommandBuffer InCmd, UHGPUQuery* InQuery, std::string InName);
	~UHGPUTimeQueryScope();

	static const std::vector<UHGPUQuery*>& GetResiteredGPUTime();
	static void ClearRegisteredGPUTime();

private:
#if WITH_EDITOR
	UHGPUQuery* GPUTimeQuery;
	VkCommandBuffer Cmd;

	// editor only registered game time, which will be displayed in profile
	static std::vector<UHGPUQuery*> RegisteredGPUTime;
#endif
};