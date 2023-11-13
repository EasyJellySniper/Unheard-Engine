#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderSkyPass(UHRenderBuilder& RenderBuilder)
{
	RenderBuilder.ResourceBarrier(GSceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	if (CurrentScene == nullptr || CurrentScene->GetSkyLight() == nullptr || !bIsSkyLightEnabledRT)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Sky Pass");
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::SkyPass]);

		RenderBuilder.BeginRenderPass(SkyboxPassObj.RenderPass, SkyboxPassObj.FrameBuffer, RenderResolution);

		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// bind state
		RenderBuilder.BindGraphicState(SkyPassShader->GetState());

		// bind sets
		RenderBuilder.BindDescriptorSet(SkyPassShader->GetPipelineLayout(), SkyPassShader->GetDescriptorSet(CurrentFrameRT));

		// draw skybox renderer
		RenderBuilder.BindVertexBuffer(SkyMeshRT->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(SkyMeshRT);
		RenderBuilder.DrawIndexed(SkyMeshRT->GetIndicesCount());

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}