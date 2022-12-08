#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderDepthPrePass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || !bEnableDepthPrePass)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Depth Pre Pass");

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::DepthPrePass]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	VkClearValue DepthClearValue;
	DepthClearValue.depthStencil.depth = 0.0f;
	DepthClearValue.depthStencil.stencil = 0;

	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// begin as secondary
	GraphBuilder.BeginRenderPass(DepthPassObj.RenderPass, DepthPassObj.FrameBuffer, RenderResolution, DepthClearValue);

	// get all opaque renderers from scene
	std::vector<VkCommandBuffer> CmdToExecute;
	for (const UHMeshRendererComponent* Renderer : OpaquesToRender)
	{
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();

		// skip materials which are not default lit
		if (Mat->GetShadingModel() != UHShadingModel::DefaultLit)
		{
			continue;
		}

		int32_t RendererIdx = Renderer->GetBufferDataIndex();

	#if WITH_DEBUG
		if (DepthPassShaders.find(RendererIdx) == DepthPassShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[RenderDepthPass] Can't find base pass shader for material: \n");
			continue;
		}
	#endif

		const UHDepthPassShader& DepthShader = DepthPassShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(DepthShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(DepthShader.GetPipelineLayout(), DepthShader.GetDescriptorSet(CurrentFrame));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndRenderPass();

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::DepthPrePass]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}