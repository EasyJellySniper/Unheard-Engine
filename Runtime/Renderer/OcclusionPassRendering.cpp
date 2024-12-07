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

void UHDeferredShadingRenderer::ResolveOcclusionResult(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("ResolveOcclusionResult", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::OcclusionResolve)], "ResolveOcclusionResult");

	// Resolve the previous frame result to the buffer. 
	// Allow partial update so it won't stuck for the individuals that don't have Begin/EndQuery() called.
	// If this still failed to get the proper results, consider copying the result individually with parallel tasks and setting
	// VK_QUERY_RESULT_WAIT_BIT flag for each query.

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Resolve Occlusion Result");

	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	vkCmdCopyQueryPoolResults(RenderBuilder.GetCmdList()
		, OcclusionQuery[PrevFrame]->GetQueryPool()
		, 0
		, OcclusionQuery[PrevFrame]->GetQueryCount()
		, GOcclusionResult[PrevFrame]->GetBuffer()
		, 0
		, sizeof(uint32_t)
		, VK_QUERY_RESULT_PARTIAL_BIT);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
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
		RenderBuilder.BeginOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);

		RenderBuilder.BindGraphicState(UHOcclusionPassShader::GetOcclusionState());
		RenderBuilder.BindVertexBuffer(CubeMesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(CubeMesh);
		RenderBuilder.BindDescriptorSet(OcclusionShader->GetPipelineLayout(), OcclusionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(CubeMesh->GetIndicesCount());
		RenderBuilder.EndOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();

#if WITH_EDITOR
	ThreadOccludedCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}