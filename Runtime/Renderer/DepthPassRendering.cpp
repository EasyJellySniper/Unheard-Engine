#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderDepthPrePass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::DepthPrePass]);
	if (CurrentScene == nullptr || !bEnableDepthPrePass)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Depth Pre Pass");
	{
		VkClearValue DepthClearValue;
		DepthClearValue.depthStencil.depth = 0.0f;
		DepthClearValue.depthStencil.stencil = 0;

		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// begin render pass based on flag
		if (bParallelSubmissionRT)
		{
			RenderBuilder.BeginRenderPass(DepthPassObj, RenderResolution, DepthClearValue, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		}
		else
		{
			RenderBuilder.BeginRenderPass(DepthPassObj, RenderResolution, DepthClearValue);
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
			ParallelTask = UHParallelTask::DepthPassTask;
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
			RenderBuilder.ExecuteBundles(DepthParallelSubmitter.WorkerBundles);
		}
		else
		{
			// bind texture table, they should only be bound once
			if (DepthPassShaders.size() > 0)
			{
				std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
					, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
				RenderBuilder.BindDescriptorSet(DepthPassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
			}

			// render all opaque renderers from scene
			for (const UHMeshRendererComponent* Renderer : OpaquesToRender)
			{
				const UHMaterial* Mat = Renderer->GetMaterial();
				UHMesh* Mesh = Renderer->GetMesh();

				int32_t RendererIdx = Renderer->GetBufferDataIndex();

#if WITH_EDITOR
				if (DepthPassShaders.find(RendererIdx) == DepthPassShaders.end())
				{
					// unlikely to happen, but printing a message for debug
					UHE_LOG(L"[RenderDepthPass] Can't find base pass shader for material: \n");
					continue;
				}
#endif

				const UHDepthPassShader* DepthShader = DepthPassShaders[RendererIdx].get();

				GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
					std::to_string(Mesh->GetIndicesCount() / 3) + ")");

				// bind pipelines
				RenderBuilder.BindGraphicState(DepthShader->GetState());
				RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
				RenderBuilder.BindIndexBuffer(Mesh);
				RenderBuilder.BindDescriptorSet(DepthShader->GetPipelineLayout(), DepthShader->GetDescriptorSet(CurrentFrameRT));

				// draw call
				RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

				GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
			}
		}

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

// depth pass task, called by worker thread
void UHDeferredShadingRenderer::DepthPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(OpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = DepthPassObj.RenderPass;
	InheritanceInfo.framebuffer = DepthPassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, DepthParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	// bind texture table, they should only be bound once
	if (DepthPassShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(DepthPassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = OpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHDepthPassShader* DepthShader = DepthPassShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		RenderBuilder.BindGraphicState(DepthShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(DepthShader->GetPipelineLayout(), DepthShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}

void UHDeferredShadingRenderer::DownsampleDepthPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::DownsampleDepthPass]);

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Downsample Depth Pass");
	{
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GHalfDepth, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GHalfTranslucentDepth, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.FlushResourceBarrier();

		RenderBuilder.BindComputeState(DownsampleDepthShader->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(DownsampleDepthShader->GetPipelineLayout(), DownsampleDepthShader->GetDescriptorSet(CurrentFrameRT));

		VkExtent2D DownsampleDepthResolution;
		DownsampleDepthResolution.width = RenderResolution.width >> 1;
		DownsampleDepthResolution.height = RenderResolution.height >> 1;
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(DownsampleDepthResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(DownsampleDepthResolution.height, GThreadGroup2D_Y), 1);

		RenderBuilder.PushResourceBarrier(UHImageBarrier(GHalfDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GHalfTranslucentDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}