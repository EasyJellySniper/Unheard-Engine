#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::OcclusionQueryReset(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("OcclusionQueryReset", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::OcclusionReset)], "OcclusionReset");
	if (CurrentScene == nullptr || !bEnableHWOcclusionRT)
	{
		return;
	}

	// fetch the occlusion result from the previous query, it should be available after EndQuery() calls.
	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;

	vkCmdCopyQueryPoolResults(RenderBuilder.GetCmdList()
		, OcclusionQuery[PrevFrame]->GetQueryPool()
		, 0
		, OcclusionQuery[PrevFrame]->GetQueryCount()
		, GOcclusionResult[PrevFrame]->GetBuffer()
		, 0
		, sizeof(uint32_t)
		, VK_QUERY_RESULT_PARTIAL_BIT);

	// reset occlusion query for the current frame
	vkResetQueryPool(GraphicInterface->GetLogicalDevice(), OcclusionQuery[CurrentFrameRT]->GetQueryPool(), 0, OcclusionQuery[CurrentFrameRT]->GetQueryCount());
}

void UHDeferredShadingRenderer::RenderOcclusionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderOcclusionPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::OcclusionPass)], "OcclusionPass");
	if (CurrentScene == nullptr || !bEnableHWOcclusionRT)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Occlusion Pass");
	{
		RenderBuilder.BeginRenderPass(OcclusionPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
		for (int32_t I = 0; I < NumWorkerThreads; I++)
		{
			ThreadOccludedCalls[I] = 0;
		}
#endif

		// wake all worker threads
		ParallelTask = UHParallelTask::OcclusionPassTask;
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
			RenderBuilder.OccludedCalls += ThreadOccludedCalls[I];
		}
#endif

		// execute all recorded batches
		RenderBuilder.ExecuteBundles(OcclusionParallelSubmitter);

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::OcclusionPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(OcclusionRenderers.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = OcclusionPassObj.RenderPass;
	InheritanceInfo.framebuffer = OcclusionPassObj.FrameBuffer;
	InheritanceInfo.occlusionQueryEnable = bEnableHWOcclusionRT;

	UHRenderBuilder RenderBuilder(GraphicInterface, OcclusionParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	if (StartIdx >= EndIdx)
	{
		RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
		RenderBuilder.EndCommandBuffer();
		return;
	}

	RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = OcclusionRenderers[I];
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		const UHOcclusionPassShader* OcclusionShader = OcclusionPassShaders[RendererIdx].get();
		const UHMesh* Mesh = Renderer->GetMesh();
		const int32_t TriCount = Mesh->GetIndicesCount() / 3;

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Occlusion Box");

		// bind pipelines
		const bool bOcclusionTest = bEnableHWOcclusionRT && TriCount >= OcclusionThresholdRT;
		if (bOcclusionTest)
		{
			RenderBuilder.BeginOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);
		}

		RenderBuilder.BindGraphicState(UHOcclusionPassShader::GetOcclusionState());
		RenderBuilder.BindVertexBuffer(SkyMeshRT->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(SkyMeshRT);
		RenderBuilder.BindDescriptorSet(OcclusionShader->GetPipelineLayout(), OcclusionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(SkyMeshRT->GetIndicesCount());
		if (bOcclusionTest)
		{
			RenderBuilder.EndOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);
		}

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();

#if WITH_EDITOR
	ThreadOccludedCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}