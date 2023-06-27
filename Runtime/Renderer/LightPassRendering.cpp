#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderLightPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetDirLightCount() == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Light Pass");

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::LightPass]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphBuilder.ResourceBarrier(SceneResult, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	// bind state
	UHComputeState* State = LightPassShader.GetComputeState();
	GraphBuilder.BindComputeState(State);

	// bind sets
	GraphBuilder.BindDescriptorSetCompute(LightPassShader.GetPipelineLayout(), LightPassShader.GetDescriptorSet(CurrentFrame));

	// dispatch
	GraphBuilder.Dispatch((RenderResolution.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RenderResolution.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

	GraphBuilder.ResourceBarrier(SceneResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::LightPass]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}