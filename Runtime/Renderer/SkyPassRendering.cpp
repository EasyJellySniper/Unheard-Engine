#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderSkyPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || CurrentScene->GetSkyboxRenderer() == nullptr)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Sky Pass");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::SkyPass]);

		GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		GraphBuilder.BeginRenderPass(SkyboxPassObj.RenderPass, SkyboxPassObj.FrameBuffer, RenderResolution);

		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);

		// bind state
		GraphBuilder.BindGraphicState(SkyPassShader.GetState());

		// bind sets
		GraphBuilder.BindDescriptorSet(SkyPassShader.GetPipelineLayout(), SkyPassShader.GetDescriptorSet(CurrentFrame));

		// draw skybox renderer
		const UHMeshRendererComponent* SkyboxRenderer = CurrentScene->GetSkyboxRenderer();
		const UHMaterial* Mat = SkyboxRenderer->GetMaterial();
		UHMesh* Mesh = SkyboxRenderer->GetMesh();

		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}