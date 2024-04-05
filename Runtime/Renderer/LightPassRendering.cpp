#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::DispatchLightCulling(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::LightCulling)]);
	if (CurrentScene == nullptr || (CurrentScene->GetPointLightCount() == 0 && CurrentScene->GetSpotLightCount() == 0))
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Light Culling");
	{
		// clear the point light/spot light list buffer
		RenderBuilder.ClearUAVBuffer(GPointLightListBuffer->GetBuffer(), 0);
		RenderBuilder.ClearUAVBuffer(GPointLightListTransBuffer->GetBuffer(), 0);
		RenderBuilder.ClearUAVBuffer(GSpotLightListBuffer->GetBuffer(), 0);
		RenderBuilder.ClearUAVBuffer(GSpotLightListTransBuffer->GetBuffer(), 0);

		// bind state
		UHComputeState* State = LightCullingShader->GetComputeState();
		RenderBuilder.BindComputeState(State);

		// bind sets
		RenderBuilder.BindDescriptorSetCompute(LightCullingShader->GetPipelineLayout(), LightCullingShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch light culliong
		uint32_t TileCountX, TileCountY;
		GetLightCullingTileCount(TileCountX, TileCountY);
		RenderBuilder.Dispatch(TileCountX, TileCountY, 1);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::RenderLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::LightPass)]);
	if (CurrentScene == nullptr || (CurrentScene->GetDirLightCount() == 0 && CurrentScene->GetPointLightCount() && CurrentScene->GetSpotLightCount()))
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Light Pass");
	{
		RenderBuilder.ResourceBarrier(GSceneResult, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

		// bind state
		UHComputeState* State = LightPassShader->GetComputeState();
		RenderBuilder.BindComputeState(State);

		// bind sets
		RenderBuilder.BindDescriptorSetCompute(LightPassShader->GetPipelineLayout(), LightPassShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y), 1);

		RenderBuilder.ResourceBarrier(GSceneResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}