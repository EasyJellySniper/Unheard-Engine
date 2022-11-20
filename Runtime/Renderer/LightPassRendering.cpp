#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderLightPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetDirLightCount() == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Light Pass");
	GraphBuilder.BeginRenderPass(LightPassObj.RenderPass, LightPassObj.FrameBuffer, RenderResolution);

	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = LightPassShader.GetState();
	GraphBuilder.BindGraphicState(State);

	// bind sets
	GraphBuilder.BindDescriptorSet(LightPassShader.GetPipelineLayout(), LightPassShader.GetDescriptorSet(CurrentFrame));

	// doesn't need IB for full screen quad
	GraphBuilder.BindVertexBuffer(VK_NULL_HANDLE);
	GraphBuilder.DrawFullScreenQuad();

	GraphBuilder.EndRenderPass();
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}