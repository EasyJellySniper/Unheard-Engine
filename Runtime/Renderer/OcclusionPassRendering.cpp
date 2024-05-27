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

	vkCmdCopyQueryPoolResults(RenderBuilder.GetCmdList()
		, OcclusionQuery[PrevFrame]->GetQueryPool()
		, 0
		, OcclusionQuery[PrevFrame]->GetQueryCount()
		, OcclusionResultGPU[PrevFrame]->GetBuffer()
		, 0
		, sizeof(uint32_t)
		, VK_QUERY_RESULT_PARTIAL_BIT);

	// reset occlusion query for the current frame
	vkResetQueryPool(GraphicInterface->GetLogicalDevice(), OcclusionQuery[CurrentFrameRT]->GetQueryPool(), 0, OcclusionQuery[CurrentFrameRT]->GetQueryCount());
}