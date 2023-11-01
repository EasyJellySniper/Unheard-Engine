#include "DeferredShadingRenderer.h"

// implementation of RenderBasePass(), this pass is a deferred rendering with GBuffers and depth buffer
void UHDeferredShadingRenderer::RenderBasePass(UHRenderBuilder& RenderBuilder)
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
		VkClearValue DepthClear{};
		DepthClear.depthStencil = { 0.0f,0 };
		ClearValues.push_back(DepthClear);
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Base Pass");
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::BasePass]);

		// setup viewport and scissor
		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// begin render pass based on flag
		if (bParallelSubmissionRT)
		{
			RenderBuilder.BeginRenderPass(BasePassObj.RenderPass, BasePassObj.FrameBuffer, RenderResolution, ClearValues, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			RenderBuilder.BeginRenderPass(BasePassObj.RenderPass, BasePassObj.FrameBuffer, RenderResolution, ClearValues);
		}

		if (bParallelSubmissionRT)
		{
#if WITH_EDITOR
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				ThreadDrawCalls[I] = 0;
			}
#endif

			// wake all worker threads
			ParallelTask = UHParallelTask::BasePassTask;
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				WorkerThreads[I]->WakeThread();
			}

			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				WorkerThreads[I]->WaitTask();
			}

#if WITH_EDITOR
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				RenderBuilder.DrawCalls += ThreadDrawCalls[I];
			}
#endif

			// execute all recorded batches
			RenderBuilder.ExecuteBundles(BaseParallelSubmitter.WorkerBundles);
		}
		else
		{
			// bind texture table, they should only be bound once
			if (BasePassShaders.size() > 0)
			{
				std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
					, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
				RenderBuilder.BindDescriptorSet(BasePassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
			}

			// render all opaque renderers from scene
			for (const UHMeshRendererComponent* Renderer : OpaquesToRender)
			{
				const UHMaterial* Mat = Renderer->GetMaterial();
				UHMesh* Mesh = Renderer->GetMesh();
				int32_t RendererIdx = Renderer->GetBufferDataIndex();

#if WITH_EDITOR
				if (BasePassShaders.find(RendererIdx) == BasePassShaders.end())
				{
					// unlikely to happen, but printing a message for debug
					UHE_LOG(L"[RenderBasePass] Can't find base pass shader for material: \n");
					continue;
				}
#endif

				const UHBasePassShader* BaseShader = BasePassShaders[RendererIdx].get();

				GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
					std::to_string(Mesh->GetIndicesCount() / 3) + ")");

				// bind pipelines
				RenderBuilder.BindGraphicState(BaseShader->GetState());
				RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				RenderBuilder.BindIndexBuffer(Mesh);
				RenderBuilder.BindDescriptorSet(BaseShader->GetPipelineLayout(), BaseShader->GetDescriptorSet(CurrentFrameRT));

				// draw call
				RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
			}
		}

		RenderBuilder.EndRenderPass();

		// transition states of Gbuffer after base pass, they will be used in the shader
		std::vector<UHTexture*> GBuffers = { GSceneDiffuse, GSceneNormal, GSceneMaterial, GSceneMip, GSceneVertexNormal };
		RenderBuilder.ResourceBarrier(GBuffers, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
		// doesn't need to transition depth as the following motion pass will do it
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

// base pass task, called by worker thread
void UHDeferredShadingRenderer::BasePassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(OpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = BasePassObj.RenderPass;
	InheritanceInfo.framebuffer = BasePassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, BaseParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	// bind texture table, they should only be bound once
	if (BasePassShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(BasePassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = OpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHBasePassShader* BaseShader = BasePassShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		RenderBuilder.BindGraphicState(BaseShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(BaseShader->GetPipelineLayout(), BaseShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}