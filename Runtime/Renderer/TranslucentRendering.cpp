#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderTranslucentPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	// this pass doesn't need a RT clear, will draw on scene result directly
	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Translucent Pass");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::TranslucentPass]);

		// setup viewport and scissor
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);

		if (bParallelSubmissionRT)
		{
			GraphBuilder.BeginRenderPass(TranslucentPassObj.RenderPass, TranslucentPassObj.FrameBuffer, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			GraphBuilder.BeginRenderPass(TranslucentPassObj.RenderPass, TranslucentPassObj.FrameBuffer, RenderResolution);
		}

		if (bParallelSubmissionRT)
		{
#if WITH_DEBUG
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

#if WITH_DEBUG
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				GraphBuilder.DrawCalls += ThreadDrawCalls[I];
			}
#endif

			// execute all recorded batches
			GraphBuilder.ExecuteBundles(TranslucentParallelSubmitter.WorkerBundles);
		}
		else
		{
			// bind texture table, they should only be bound once
			if (TranslucentPassShaders.size() > 0)
			{
				std::vector<VkDescriptorSet> TextureTableSets = { TextureTable.GetDescriptorSet(CurrentFrame)
					, SamplerTable.GetDescriptorSet(CurrentFrame) };
				GraphBuilder.BindDescriptorSet(TranslucentPassShaders.begin()->second.GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
			}

			// render all translucent renderers from scene
			for (const UHMeshRendererComponent* Renderer : TranslucentsToRender)
			{
				const UHMaterial* Mat = Renderer->GetMaterial();
				UHMesh* Mesh = Renderer->GetMesh();
				int32_t RendererIdx = Renderer->GetBufferDataIndex();

#if WITH_DEBUG
				if (TranslucentPassShaders.find(RendererIdx) == TranslucentPassShaders.end())
				{
					// unlikely to happen, but printing a message for debug
					UHE_LOG(L"[RenderTranslucentPass] Can't find translucent pass shader for material: \n");
					continue;
				}
#endif

				const UHTranslucentPassShader& TranslucentShader = TranslucentPassShaders[RendererIdx];

				GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
					std::to_string(Mesh->GetIndicesCount() / 3) + ")");

				// bind pipelines
				GraphBuilder.BindGraphicState(TranslucentShader.GetState());
				GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				GraphBuilder.BindIndexBuffer(Mesh);
				GraphBuilder.BindDescriptorSet(TranslucentShader.GetPipelineLayout(), TranslucentShader.GetDescriptorSet(CurrentFrame));

				// draw call
				GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
			}
		}

		GraphBuilder.EndRenderPass();
		GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
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

	UHGraphicBuilder GraphBuilder(GraphicInterface, TranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrame]);
	GraphBuilder.BeginCommandBuffer(&InheritanceInfo);

	// silence the false positive error regarding vp and scissor
	if (GraphicInterface->IsDebugLayerEnabled())
	{
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);
	}

	// bind texture table, they should only be bound once
	if (TranslucentPassShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable.GetDescriptorSet(CurrentFrame)
			, SamplerTable.GetDescriptorSet(CurrentFrame) };
		GraphBuilder.BindDescriptorSet(TranslucentPassShaders.begin()->second.GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHTranslucentPassShader& TranslucentShader = TranslucentPassShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(TranslucentShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(TranslucentShader.GetPipelineLayout(), TranslucentShader.GetDescriptorSet(CurrentFrame));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndCommandBuffer();
#if WITH_DEBUG
	ThreadDrawCalls[ThreadIdx] += GraphBuilder.DrawCalls;
#endif
}