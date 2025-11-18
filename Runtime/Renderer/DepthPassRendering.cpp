#include "DeferredShadingRenderer.h"

class UHDepthPassAsyncTask : public UHAsyncTask
{
public:
	void Init(UHDeferredShadingRenderer* InRenderer)
	{
		SceneRenderer = InRenderer;
	}

	virtual void DoTask(const int32_t ThreadIdx) override
	{
		SceneRenderer->DepthPassTask(ThreadIdx);
	}

private:
	UHDeferredShadingRenderer* SceneRenderer = nullptr;
};

void UHDeferredShadingRenderer::RenderDepthPrePass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderDepthPrePass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::DepthPrePass)], "DepthPass");
	if (CurrentScene == nullptr || !RTParams.bEnableDepthPrepass)
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

		// do mesh shader version if it's supported.
		if (GraphicInterface->IsMeshShaderSupported())
		{
			RenderBuilder.BeginRenderPass(DepthPassObj, RenderResolution, DepthClearValue);

			// bindless table, they should only be bound once
			if (DepthMeshShaders.size() > 0 && SortedMeshShaderGroupIndex.size() > 0)
			{
				std::vector<VkDescriptorSet> BindlessTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
					, SamplerTable->GetDescriptorSet(CurrentFrameRT)
					, MeshletTable->GetDescriptorSet(CurrentFrameRT) 
					, PositionTable->GetDescriptorSet(CurrentFrameRT)
					, UV0Table->GetDescriptorSet(CurrentFrameRT)
					, IndicesTable->GetDescriptorSet(CurrentFrameRT)
				};
				RenderBuilder.BindDescriptorSet(DepthMeshShaders[SortedMeshShaderGroupIndex[0]]->GetPipelineLayout(), BindlessTableSets, GTextureTableSpace);
			}

			for (size_t Idx = 0; Idx < SortedMeshShaderGroupIndex.size(); Idx++)
			{
				const int32_t GroupIndex = SortedMeshShaderGroupIndex[Idx];
				const uint32_t VisibleMeshlets = static_cast<uint32_t>(VisibleMeshShaderData[GroupIndex].size());
				if (VisibleMeshlets == 0)
				{
					continue;
				}

				const UHDepthMeshShader* DepthMS = DepthMeshShaders[GroupIndex].get();

				GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatching depth pass " + DepthMS->GetMaterialCache()->GetName());

				RenderBuilder.BindGraphicState(DepthMS->GetState());
				RenderBuilder.BindDescriptorSet(DepthMS->GetPipelineLayout(), DepthMS->GetDescriptorSet(CurrentFrameRT));

				// Dispatch meshlets
				RenderBuilder.DispatchMesh(VisibleMeshlets, 1, 1);

				GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
			}
		}
		else
		{
			// begin render pass
			RenderBuilder.BeginRenderPass(DepthPassObj, RenderResolution, DepthClearValue, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
			for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
			{
				ThreadDrawCalls[I] = 0;
			}
#endif

			// init and wake all tasks
			static std::vector<UHDepthPassAsyncTask> Tasks(NumParallelRenderSubmitters);
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
			}
#endif

			// execute all recorded batches
			RenderBuilder.ExecuteBundles(DepthParallelSubmitter);
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
	const int32_t RendererCount = (MaxCount + NumParallelRenderSubmitters) / NumParallelRenderSubmitters;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumParallelRenderSubmitters - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = DepthPassObj.RenderPass;
	InheritanceInfo.framebuffer = DepthPassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, DepthParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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