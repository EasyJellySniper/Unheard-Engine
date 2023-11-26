#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderTranslucentPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::TranslucentPass]);
	if (CurrentScene == nullptr)
	{
		return;
	}

	// this pass doesn't need a RT clear, will draw on scene result directly
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Translucent Pass");
	{
		// setup viewport and scissor
		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		if (bParallelSubmissionRT)
		{
			RenderBuilder.BeginRenderPass(TranslucentPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			RenderBuilder.BeginRenderPass(TranslucentPassObj, RenderResolution);
		}

		if (bParallelSubmissionRT)
		{
#if WITH_EDITOR
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				ThreadDrawCalls[I] = 0;
			}
#endif

			// parallel pass
			ParallelTask = UHParallelTask::TranslucentPassTask;
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
			RenderBuilder.ExecuteBundles(TranslucentParallelSubmitter.WorkerBundles);
		}
		else
		{
			// bind texture table, they should only be bound once
			if (TranslucentPassShaders.size() > 0)
			{
				std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
					, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
				RenderBuilder.BindDescriptorSet(TranslucentPassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
			}

			// render all translucent renderers from scene
			for (const UHMeshRendererComponent* Renderer : TranslucentsToRender)
			{
				const UHMaterial* Mat = Renderer->GetMaterial();
				UHMesh* Mesh = Renderer->GetMesh();
				int32_t RendererIdx = Renderer->GetBufferDataIndex();

#if WITH_EDITOR
				if (TranslucentPassShaders.find(RendererIdx) == TranslucentPassShaders.end())
				{
					// unlikely to happen, but printing a message for debug
					UHE_LOG(L"[RenderTranslucentPass] Can't find translucent pass shader for material: \n");
					continue;
				}
#endif

				const UHTranslucentPassShader* TranslucentShader = TranslucentPassShaders[RendererIdx].get();

				GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
					std::to_string(Mesh->GetIndicesCount() / 3) + ")");

				// bind pipelines
				RenderBuilder.BindGraphicState(TranslucentShader->GetState());
				RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				RenderBuilder.BindIndexBuffer(Mesh);
				RenderBuilder.BindDescriptorSet(TranslucentShader->GetPipelineLayout(), TranslucentShader->GetDescriptorSet(CurrentFrameRT));

				// draw call
				RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
			}
		}

		RenderBuilder.EndRenderPass();
		RenderBuilder.ResourceBarrier(GSceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::TranslucentPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(TranslucentsToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = TranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = TranslucentPassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, TranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	// bind texture table, they should only be bound once
	if (TranslucentPassShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(TranslucentPassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHTranslucentPassShader* TranslucentShader = TranslucentPassShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		RenderBuilder.BindGraphicState(TranslucentShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(TranslucentShader->GetPipelineLayout(), TranslucentShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}