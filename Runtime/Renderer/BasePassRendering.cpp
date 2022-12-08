#include "DeferredShadingRenderer.h"

// implementation of RenderBasePass(), this pass is a deferred rendering with GBuffers and depth buffer
void UHDeferredShadingRenderer::RenderBasePass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	// setup clear value
	std::vector<VkClearValue> ClearValues;
	ClearValues.resize(GNumOfGBuffers);

	// clear GBuffer with pure black
	for (size_t Idx = 0; Idx < GNumOfGBuffers; Idx++)
	{
		ClearValues[Idx].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
	}

	// clear depth with 0 since reversed-z is used
	if (!bEnableDepthPrePass)
	{
		VkClearValue DepthClear;
		DepthClear.depthStencil = { 0.0f,0 };
		ClearValues.push_back(DepthClear);
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Base Pass");

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::BasePass]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphBuilder.BeginRenderPass(BasePassObj.RenderPass, BasePassObj.FrameBuffer, RenderResolution, ClearValues);

	// setup viewport and scissor
	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// get all opaque renderers from scene
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
		if (BasePassShaders.find(RendererIdx) == BasePassShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[RenderBasePass] Can't find base pass shader for material: \n");
			continue;
		}
	#endif

		const UHBasePassShader& BaseShader = BasePassShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(BaseShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(BaseShader.GetPipelineLayout(), BaseShader.GetDescriptorSet(CurrentFrame));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndRenderPass();

	// transition states of Gbuffer after base pass, they will be used in the shader
	std::vector<UHTexture*> GBuffers = { SceneDiffuse, SceneNormal, SceneMaterial, SceneMip };
	GraphBuilder.ResourceBarrier(GBuffers, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::BasePass]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}