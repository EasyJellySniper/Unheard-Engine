#include "GPUQuery.h"
#include "../Engine/Graphic.h"

UHGPUQuery::UHGPUQuery()
	: QueryCount(0)
	, State(UHGPUQueryState::Idle)
	, QueryPool(VK_NULL_HANDLE)
	, PreviousValidTimeStamp(0)
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
#if WITH_DEBUG
	if (!GEnableGPUTiming)
	{
		return;
	}
#endif

	if (State == UHGPUQueryState::Idle)
	{
		vkResetQueryPool(LogicalDevice, QueryPool, 0, QueryCount);
		vkCmdWriteTimestamp(InBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, 0);
	}
}

void UHGPUQuery::EndTimeStamp(VkCommandBuffer InBuffer)
{
#if WITH_DEBUG
	if (!GEnableGPUTiming)
	{
		return;
	}
#endif

	if (State == UHGPUQueryState::Idle)
	{
		vkCmdWriteTimestamp(InBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, 1);
		State = UHGPUQueryState::Requested;
	}
}

float UHGPUQuery::GetTimeStamp(VkCommandBuffer InBuffer)
{
#if WITH_DEBUG
	if (!GEnableGPUTiming)
	{
		return 0.0f;
	}
#endif

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
			State = Idle;
			PreviousValidTimeStamp = Duration;
			return Duration;
		}
	}

	// return previous valid time stamp, I don't want to see the value keeps jumping with 0
	return PreviousValidTimeStamp;
}