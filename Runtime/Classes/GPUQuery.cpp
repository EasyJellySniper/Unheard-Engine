#include "GPUQuery.h"
#include "../Engine/Graphic.h"

#if WITH_EDITOR
std::mutex GGPUTimeScopeLock;
std::vector<UHGPUQuery*> UHGPUTimeQueryScope::RegisteredGPUTime;
#endif

UHGPUQuery::UHGPUQuery()
	: QueryCount(0)
	, State(UHGPUQueryState::Idle)
	, QueryPool(nullptr)
#if WITH_EDITOR
	, PreviousValidTimeStamp(0)
#endif
{

}

void UHGPUQuery::Release()
{
	vkDestroyQueryPool(LogicalDevice, QueryPool, nullptr);
}

void UHGPUQuery::CreateQueryPool(uint32_t InQueryCount, VkQueryType QueryType)
{
	QueryCount = InQueryCount;

	// force query count to 2 if it's time stamp
	if (QueryType == VK_QUERY_TYPE_TIMESTAMP)
	{
		QueryCount = 2;
	}

	VkQueryPoolCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	CreateInfo.queryCount = QueryCount;
	CreateInfo.queryType = QueryType;

	if (vkCreateQueryPool(LogicalDevice, &CreateInfo, nullptr, &QueryPool) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create query pool!\n");
	}
}

void UHGPUQuery::BeginTimeStamp(VkCommandBuffer InBuffer)
{
#if WITH_EDITOR
	if (!GEnableGPUTiming)
	{
		return;
	}

	if (State == UHGPUQueryState::Idle)
	{
		vkResetQueryPool(LogicalDevice, QueryPool, 0, QueryCount);
		vkCmdWriteTimestamp(InBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, 0);
	}
#endif
}

void UHGPUQuery::EndTimeStamp(VkCommandBuffer InBuffer)
{
#if WITH_EDITOR
	if (!GEnableGPUTiming)
	{
		return;
	}

	if (State == UHGPUQueryState::Idle)
	{
		vkCmdWriteTimestamp(InBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, 1);
		State = UHGPUQueryState::Requested;
	}
#endif
}

void UHGPUQuery::ResolveTimeStamp(VkCommandBuffer InBuffer)
{
#if WITH_EDITOR
	if (!GEnableGPUTiming)
	{
		return;
	}

	// try getting time stamp after request
	if (State == UHGPUQueryState::Requested)
	{
		uint32_t Queries[3] = { 0 };

		if (vkGetQueryPoolResults(LogicalDevice, QueryPool, 0, QueryCount, 3 * sizeof(uint32_t), &Queries, sizeof(uint32_t), VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
			== VK_SUCCESS)
		{
			// get data successfully, calculate time period and return
			// also set state to Idle for next request
			float Duration = static_cast<float>(Queries[1] - Queries[0]) * GfxCache->GetGPUTimeStampPeriod() * 1e-6f;
			State = UHGPUQueryState::Idle;
			PreviousValidTimeStamp = Duration;
		}
	}
#endif
}

VkQueryPool UHGPUQuery::GetQueryPool() const
{
	return QueryPool;
}

uint32_t UHGPUQuery::GetQueryCount() const
{
	return QueryCount;
}

#if WITH_EDITOR
void UHGPUQuery::SetDebugName(const std::string& InName)
{
	Name = InName;
}

std::string UHGPUQuery::GetDebugName() const
{
	return Name;
}

float UHGPUQuery::GetLastTimeStamp() const
{
	return PreviousValidTimeStamp;
}
#endif

UHGPUTimeQueryScope::UHGPUTimeQueryScope(VkCommandBuffer InCmd, UHGPUQuery* InQuery, std::string InName)
{
#if WITH_EDITOR
	Cmd = InCmd;
	GPUTimeQuery = InQuery;

	if (GPUTimeQuery)
	{
		GPUTimeQuery->BeginTimeStamp(Cmd);
		GPUTimeQuery->SetDebugName(InName);

		// add to registered list for profiling display
		std::unique_lock<std::mutex> Lock(GGPUTimeScopeLock);
		RegisteredGPUTime.push_back(GPUTimeQuery);
	}
#endif
}

UHGPUTimeQueryScope::~UHGPUTimeQueryScope()
{
#if WITH_EDITOR
	if (GPUTimeQuery)
	{
		GPUTimeQuery->EndTimeStamp(Cmd);
	}
#endif
}

std::vector<UHGPUQuery*> UHGPUTimeQueryScope::GetResiteredGPUTime()
{
#if WITH_EDITOR
	std::unique_lock<std::mutex> Lock(GGPUTimeScopeLock);
	return RegisteredGPUTime;
#endif
}

void UHGPUTimeQueryScope::ClearRegisteredGPUTime()
{
#if WITH_EDITOR
	std::unique_lock<std::mutex> Lock(GGPUTimeScopeLock);
	RegisteredGPUTime.clear();
#endif
}