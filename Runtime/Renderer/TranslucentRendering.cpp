#include "DeferredShadingRenderer.h"

class UHTranslucentPassAsyncTask : public UHAsyncTask
{
public:
	void Init(UHDeferredShadingRenderer* InRenderer)
	{
		SceneRenderer = InRenderer;
	}

	virtual void DoTask(const int32_t ThreadIdx) override
	{
		SceneRenderer->TranslucentPassTask(ThreadIdx);
	}

private:
	UHDeferredShadingRenderer* SceneRenderer = nullptr;
};

void UHDeferredShadingRenderer::ScreenshotForRefraction(std::string PassName, UHRenderBuilder& RenderBuilder)
{
	// blit the scene result to opaque scene result
	const VkImageLayout PrevSceneResultLayout = GSceneResult->GetImageLayout();
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneResult, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();

	RenderBuilder.Blit(GSceneResult, GOpaqueSceneResult);

	// final transition
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneResult, PrevSceneResultLayout));
	RenderBuilder.FlushResourceBarrier();
}

void UHDeferredShadingRenderer::RenderTranslucentPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderTranslucentPass", false);
	if (CurrentScene == nullptr)
	{
		return;
	}
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::TranslucentPass)], "TranslucentPass");

	// this pass doesn't need a RT clear, will draw on scene result directly
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Translucent Pass");
	{
		// setup viewport and scissor
		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// Draw the translucent background
		{
			RenderBuilder.BeginRenderPass(TranslucentPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
#if WITH_EDITOR
			for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
			{
				ThreadDrawCalls[I] = 0;
				ThreadOccludedCalls[I] = 0;
			}
#endif

			// init and wake tasks
			static std::vector<UHTranslucentPassAsyncTask> Tasks(NumParallelRenderSubmitters);
			for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
			{
				Tasks[I].Init(this);
				WorkerThreads[I]->ScheduleTask(&Tasks[I]);
				WorkerThreads[I]->WakeThread();
			}

			for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
			{
				WorkerThreads[I]->WaitTask();
			}

#if WITH_EDITOR
			for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
			{
				RenderBuilder.DrawCalls += ThreadDrawCalls[I];
				RenderBuilder.OccludedCalls += ThreadOccludedCalls[I];
			}
#endif
			// execute all recorded batches
			RenderBuilder.ExecuteBundles(TranslucentParallelSubmitter);
			RenderBuilder.EndRenderPass();
		}
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::TranslucentPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(TranslucentsToRender.size());
	const int32_t RendererCount = (MaxCount + NumParallelRenderSubmitters) / NumParallelRenderSubmitters;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumParallelRenderSubmitters - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = TranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = TranslucentPassObj.FrameBuffer;
	InheritanceInfo.occlusionQueryEnable = RTParams.bEnableOcclusionQuery;

	UHRenderBuilder RenderBuilder(GraphicInterface, TranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	if (StartIdx >= EndIdx)
	{
		RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
		RenderBuilder.EndCommandBuffer();
		return;
	}

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

	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		UHMesh* Mesh = Renderer->GetMesh();
		const int32_t TriCount = Mesh->GetIndicesCount() / 3;

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(TriCount) + ")");

		// occlusion test for big meshes
		const bool bOcclusionTest = RTParams.bEnableOcclusionQuery 
			&& TriCount >= RTParams.OcclusionThreshold && !Renderer->IsCameraInsideThisRenderer();
		if (bOcclusionTest)
		{
			RenderBuilder.BeginPredication(RendererIdx, GOcclusionResult[PrevFrame]->GetBuffer());
		}

		// draw mesh
		const UHTranslucentPassShader* TranslucentShader = TranslucentPassShaders[RendererIdx].get();
		RenderBuilder.BindGraphicState(TranslucentShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(TranslucentShader->GetPipelineLayout(), TranslucentShader->GetDescriptorSet(CurrentFrameRT));

		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		if (bOcclusionTest)
		{
			RenderBuilder.EndPredication();
		}

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();

#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
	ThreadOccludedCalls[ThreadIdx] += RenderBuilder.OccludedCalls;
#endif
}