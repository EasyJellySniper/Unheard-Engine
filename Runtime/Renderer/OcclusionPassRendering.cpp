#include "DeferredShadingRenderer.h"

class UHOcclusionPassAsyncTask : public UHAsyncTask
{
public:
	void Init(UHDeferredShadingRenderer* InRenderer)
	{
		SceneRenderer = InRenderer;
	}

	virtual void DoTask(const int32_t ThreadIdx) override
	{
		SceneRenderer->OcclusionPassTask(ThreadIdx);
	}

private:
	UHDeferredShadingRenderer* SceneRenderer = nullptr;
};

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
		// reset occlusion query for the current frame
		RenderBuilder.ResetGPUQuery(OcclusionQuery[CurrentFrameRT]);
		RenderBuilder.BeginRenderPass(OcclusionPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
		for (int32_t I = 0; I < NumWorkerThreads; I++)
		{
			ThreadOccludedCalls[I] = 0;
		}
#endif

		// init and wake all tasks
		static UHOcclusionPassAsyncTask Tasks[GMaxWorkerThreads];
		for (int32_t I = 0; I < NumWorkerThreads; I++)
		{
			Tasks[I].Init(this);
			WorkerThreads[I]->ScheduleTask(&Tasks[I]);
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

		// The vkCmdEndQuery completes the query in queryPool identified by query, and marks it as available.
		// It shall be able to resolve the result to the buffer now. Allow partial update.
		vkCmdCopyQueryPoolResults(RenderBuilder.GetCmdList()
			, OcclusionQuery[CurrentFrameRT]->GetQueryPool()
			, 0
			, OcclusionQuery[CurrentFrameRT]->GetQueryCount()
			, GOcclusionResult[CurrentFrameRT]->GetBuffer()
			, 0
			, sizeof(uint32_t)
			, VK_QUERY_RESULT_PARTIAL_BIT);
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

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Occlusion Box");

		// bind pipelines
		if (bEnableHWOcclusionRT)
		{
			RenderBuilder.BeginOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);
		}

		RenderBuilder.BindGraphicState(UHOcclusionPassShader::GetOcclusionState());
		RenderBuilder.BindVertexBuffer(SkyMeshRT->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(SkyMeshRT);
		RenderBuilder.BindDescriptorSet(OcclusionShader->GetPipelineLayout(), OcclusionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(SkyMeshRT->GetIndicesCount());
		if (bEnableHWOcclusionRT)
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