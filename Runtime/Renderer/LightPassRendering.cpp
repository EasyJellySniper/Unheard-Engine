#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::DispatchLightCulling(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetPointLightCount() == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Dispatch Light Culling");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::LightCulling]);

		// clear the point light list buffer
		GraphBuilder.ClearUAVBuffer(PointLightListBuffer->GetBuffer(), 0);
		GraphBuilder.ClearUAVBuffer(PointLightListTransBuffer->GetBuffer(), 0);

		// bind state
		UHComputeState* State = LightCullingShader->GetComputeState();
		GraphBuilder.BindComputeState(State);

		// bind sets
		GraphBuilder.BindDescriptorSetCompute(LightCullingShader->GetPipelineLayout(), LightCullingShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch light culliong
		uint32_t TileCountX, TileCountY;
		GetLightCullingTileCount(TileCountX, TileCountY);
		GraphBuilder.Dispatch(TileCountX, TileCountY, 1);
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::RenderLightPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetDirLightCount() == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Light Pass");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::LightPass]);

		GraphBuilder.ResourceBarrier(SceneResult, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

		// bind state
		UHComputeState* State = LightPassShader->GetComputeState();
		GraphBuilder.BindComputeState(State);

		// bind sets
		GraphBuilder.BindDescriptorSetCompute(LightPassShader->GetPipelineLayout(), LightPassShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch
		GraphBuilder.Dispatch((RenderResolution.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RenderResolution.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

		GraphBuilder.ResourceBarrier(SceneResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}