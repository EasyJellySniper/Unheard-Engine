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
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::BasePass]);

		// setup viewport and scissor
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);

		// begin render pass based on flag
		if (bParallelSubmissionRT)
		{
			GraphBuilder.BeginRenderPass(BasePassObj.RenderPass, BasePassObj.FrameBuffer, RenderResolution, ClearValues, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			GraphBuilder.BeginRenderPass(BasePassObj.RenderPass, BasePassObj.FrameBuffer, RenderResolution, ClearValues);
		}

		if (bParallelSubmissionRT)
		{
#if WITH_DEBUG
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				ThreadDrawCalls[I] = 0;
			}
#endif

			// wake all worker threads
			RenderTask = UHRenderTask::BasePassTask;
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				WorkerThreads[I]->WakeThread();
			}

			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				WorkerThreads[I]->WaitTask();
			}

#if WITH_DEBUG
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				GraphBuilder.DrawCalls += ThreadDrawCalls[I];
			}
#endif

			// execute all recorded batches
			GraphBuilder.ExecuteBundles(BaseParallelSubmitter.WorkerBundles);
		}
		else
		{
			// render all opaque renderers from scene
			for (const UHMeshRendererComponent* Renderer : OpaquesToRender)
			{
				const UHMaterial* Mat = Renderer->GetMaterial();
				UHMesh* Mesh = Renderer->GetMesh();
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
				std::vector<VkDescriptorSet> DescriptorSets = { BaseShader.GetDescriptorSet(CurrentFrame)
					, TextureTable.GetDescriptorSet(CurrentFrame)
					, SamplerTable.GetDescriptorSet(CurrentFrame) };

				GraphBuilder.BindGraphicState(BaseShader.GetState());
				GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				GraphBuilder.BindIndexBuffer(Mesh);
				GraphBuilder.BindDescriptorSet(BaseShader.GetPipelineLayout(), DescriptorSets);

				// draw call
				GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
			}
		}

		GraphBuilder.EndRenderPass();

		// transition states of Gbuffer after base pass, they will be used in the shader
		std::vector<UHTexture*> GBuffers = { SceneDiffuse, SceneNormal, SceneMaterial, SceneMip };
		GraphBuilder.ResourceBarrier(GBuffers, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}

// base pass task, called by worker thread
void UHDeferredShadingRenderer::BasePassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	int32_t RendererCount = static_cast<int32_t>(OpaquesToRender.size()) / NumWorkerThreads;
	int32_t StartIdx = RendererCount * ThreadIdx;
	int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? static_cast<int32_t>(OpaquesToRender.size()) : StartIdx + RendererCount;

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = BasePassObj.RenderPass;
	InheritanceInfo.framebuffer = BasePassObj.FrameBuffer;

	UHGraphicBuilder GraphBuilder(GraphicInterface, BaseParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrame]);
	GraphBuilder.BeginCommandBuffer(&InheritanceInfo);

	// silence the false positive error regarding vp and scissor
	if (GraphicInterface->IsDebugLayerEnabled())
	{
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = OpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHBasePassShader& BaseShader = BasePassShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		std::vector<VkDescriptorSet> DescriptorSets = { BaseShader.GetDescriptorSet(CurrentFrame)
			, TextureTable.GetDescriptorSet(CurrentFrame)
			, SamplerTable.GetDescriptorSet(CurrentFrame) };

		GraphBuilder.BindGraphicState(BaseShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(BaseShader.GetPipelineLayout(), DescriptorSets);

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndCommandBuffer();
#if WITH_DEBUG
	ThreadDrawCalls[ThreadIdx] += GraphBuilder.DrawCalls;
#endif
}