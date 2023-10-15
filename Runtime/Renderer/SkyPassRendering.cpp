#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderSkyPass(UHRenderBuilder& RenderBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetSkyboxRenderer() == nullptr)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Sky Pass");
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::SkyPass]);

		RenderBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		RenderBuilder.BeginRenderPass(SkyboxPassObj.RenderPass, SkyboxPassObj.FrameBuffer, RenderResolution);

		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// bind state
		RenderBuilder.BindGraphicState(SkyPassShader->GetState());

		// bind sets
		RenderBuilder.BindDescriptorSet(SkyPassShader->GetPipelineLayout(), SkyPassShader->GetDescriptorSet(CurrentFrameRT));

		// draw skybox renderer
		const UHMeshRendererComponent* SkyboxRenderer = CurrentScene->GetSkyboxRenderer();
		const UHMaterial* Mat = SkyboxRenderer->GetMaterial();
		UHMesh* Mesh = SkyboxRenderer->GetMesh();

		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}