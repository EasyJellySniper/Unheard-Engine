#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::OcclusionQueryReset(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("OcclusionQueryReset", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::OcclusionReset)]);
	if (CurrentScene == nullptr || !bEnableHWOcclusionRT)
	{
		return;
	}

	// fetch the occlusion result from the previous query, it should be available after EndQuery() calls.
	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	const size_t DataSize = sizeof(uint32_t) * OcclusionResult[PrevFrame].size();

	vkGetQueryPoolResults(GraphicInterface->GetLogicalDevice()
		, OcclusionQuery[PrevFrame]->GetQueryPool()
		, 0
		, OcclusionQuery[PrevFrame]->GetQueryCount()
		, DataSize
		, OcclusionResult[PrevFrame].data()
		, sizeof(uint32_t)
		, VK_QUERY_RESULT_PARTIAL_BIT);

	// reset occlusion query for the current frame
	std::fill(OcclusionResult[CurrentFrameRT].begin(), OcclusionResult[CurrentFrameRT].end(), 1);
	vkResetQueryPool(GraphicInterface->GetLogicalDevice(), OcclusionQuery[CurrentFrameRT]->GetQueryPool(), 0, OcclusionQuery[CurrentFrameRT]->GetQueryCount());
}