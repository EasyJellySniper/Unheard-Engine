#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderDepthPrePass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr || !bEnableDepthPrePass)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Depth Pre Pass");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::DepthPrePass]);

		VkClearValue DepthClearValue;
		DepthClearValue.depthStencil.depth = 0.0f;
		DepthClearValue.depthStencil.stencil = 0;

		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);

		// begin render pass based on flag
		if (bParallelSubmissionRT)
		{
			GraphBuilder.BeginRenderPass(DepthPassObj.RenderPass, DepthPassObj.FrameBuffer, RenderResolution, DepthClearValue, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			GraphBuilder.BeginRenderPass(DepthPassObj.RenderPass, DepthPassObj.FrameBuffer, RenderResolution, DepthClearValue);
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
			ParallelTask = UHParallelTask::DepthPassTask;
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
			GraphBuilder.ExecuteBundles(DepthParallelSubmitter.WorkerBundles);
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
				std::vector<VkDescriptorSet> DescriptorSets = { DepthShader.GetDescriptorSet(CurrentFrame)
					, TextureTable.GetDescriptorSet(CurrentFrame)
					, SamplerTable.GetDescriptorSet(CurrentFrame) };

				GraphBuilder.BindGraphicState(DepthShader.GetState());
				GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				GraphBuilder.BindIndexBuffer(Mesh);
				GraphBuilder.BindDescriptorSet(DepthShader.GetPipelineLayout(), DescriptorSets);

				// draw call
				GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
			}
		}

		GraphBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}

// depth pass task, called by worker thread
void UHDeferredShadingRenderer::DepthPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	int32_t RendererCount = static_cast<int32_t>(OpaquesToRender.size()) / NumWorkerThreads;
	int32_t StartIdx = RendererCount * ThreadIdx;
	int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? static_cast<int32_t>(OpaquesToRender.size()) : StartIdx + RendererCount;

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = DepthPassObj.RenderPass;
	InheritanceInfo.framebuffer = DepthPassObj.FrameBuffer;

	UHGraphicBuilder GraphBuilder(GraphicInterface, DepthParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrame]);
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

		const UHDepthPassShader& DepthShader = DepthPassShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		std::vector<VkDescriptorSet> DescriptorSets = { DepthShader.GetDescriptorSet(CurrentFrame)
			, TextureTable.GetDescriptorSet(CurrentFrame)
			, SamplerTable.GetDescriptorSet(CurrentFrame) };

		GraphBuilder.BindGraphicState(DepthShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(DepthShader.GetPipelineLayout(), DescriptorSets);

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndCommandBuffer();
#if WITH_DEBUG
	ThreadDrawCalls[ThreadIdx] += GraphBuilder.DrawCalls;
#endif
}