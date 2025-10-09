#include "DeferredShadingRenderer.h"

class UHBasePassAsyncTask : public UHAsyncTask
{
public:
	void Init(UHDeferredShadingRenderer* InRenderer)
	{
		SceneRenderer = InRenderer;
	}

	virtual void DoTask(const int32_t ThreadIdx) override
	{
		SceneRenderer->BasePassTask(ThreadIdx);
	}

private:
	UHDeferredShadingRenderer* SceneRenderer = nullptr;
};

// implementation of RenderBasePass(), this pass is a deferred rendering with GBuffers and depth buffer
void UHDeferredShadingRenderer::RenderBasePass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderBasePass", false);
	if (CurrentScene == nullptr)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::BasePass)], "BasePass");

	// setup clear value
	std::vector<VkClearValue> ClearValues;
	ClearValues.resize(GNumOfGBuffers);

	// clear GBuffer with pure black
	for (size_t Idx = 0; Idx < GNumOfGBuffers; Idx++)
	{
		ClearValues[Idx].color = { {0.0f, 0.0f, 0.0f, 0.0f} };
	}

	// clear depth with 0 since reversed-z is used
	if (!RTParams.bEnableDepthPrepass)
	{
		VkClearValue DepthClear{};
		DepthClear.depthStencil = { 0.0f,0 };
		ClearValues.push_back(DepthClear);
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Base Pass");
	{
		// setup viewport and scissor
		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// do mesh shader version if it's supported.
		if (GraphicInterface->IsMeshShaderSupported())
		{
			RenderBuilder.BeginRenderPass(BasePassObj, RenderResolution, ClearValues);
			// bindless table, they should only be bound once
			if (BaseMeshShaders.size() > 0 && SortedMeshShaderGroupIndex.size() > 0)
			{
				std::vector<VkDescriptorSet> BindlessTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
					, SamplerTable->GetDescriptorSet(CurrentFrameRT)
					, MeshletTable->GetDescriptorSet(CurrentFrameRT)
					, PositionTable->GetDescriptorSet(CurrentFrameRT)
					, UV0Table->GetDescriptorSet(CurrentFrameRT)
					, IndicesTable->GetDescriptorSet(CurrentFrameRT)
					, NormalTable->GetDescriptorSet(CurrentFrameRT)
					, TangentTable->GetDescriptorSet(CurrentFrameRT)
				};
				RenderBuilder.BindDescriptorSet(BaseMeshShaders[SortedMeshShaderGroupIndex[0]]->GetPipelineLayout(), BindlessTableSets, GTextureTableSpace);
			}

			for (size_t Idx = 0; Idx < SortedMeshShaderGroupIndex.size(); Idx++)
			{
				const int32_t GroupIndex = SortedMeshShaderGroupIndex[Idx];
				const uint32_t VisibleMeshlets = static_cast<uint32_t>(VisibleMeshShaderData[GroupIndex].size());
				if (VisibleMeshlets == 0)
				{
					continue;
				}

				const UHBaseMeshShader* BaseMS = BaseMeshShaders[GroupIndex].get();

				GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatching base pass " + BaseMS->GetMaterialCache()->GetName());

				RenderBuilder.BindGraphicState(BaseMS->GetState());
				RenderBuilder.BindDescriptorSet(BaseMS->GetPipelineLayout(), BaseMS->GetDescriptorSet(CurrentFrameRT));

				// Dispatch meshlets
				RenderBuilder.DispatchMesh(VisibleMeshlets, 1, 1);

				GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
			}
		}
		else
		{
			// begin render pass
			RenderBuilder.BeginRenderPass(BasePassObj, RenderResolution, ClearValues, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				ThreadDrawCalls[I] = 0;
				ThreadOccludedCalls[I] = 0;
			}
#endif

			// wake all worker threads
			static UHBasePassAsyncTask Tasks[GMaxWorkerThreads];
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
				RenderBuilder.DrawCalls += ThreadDrawCalls[I];
				RenderBuilder.OccludedCalls += ThreadOccludedCalls[I];
			}
#endif

			// execute all recorded batches
			RenderBuilder.ExecuteBundles(BaseParallelSubmitter);
		}
		RenderBuilder.EndRenderPass();

		// transition states of Gbuffer after base pass, they will be used in the shader
		std::vector<UHTexture*> GBuffers = { GSceneDiffuse, GSceneNormal, GSceneMaterial, GSceneMip, GSceneData };
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
	InheritanceInfo.occlusionQueryEnable = RTParams.bEnableOcclusionQuery;

	UHRenderBuilder RenderBuilder(GraphicInterface, BaseParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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
	if (BasePassShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(BasePassShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		const UHMeshRendererComponent* Renderer = OpaquesToRender[I];
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
		const UHBasePassShader* BaseShader = BasePassShaders[RendererIdx].get();
		RenderBuilder.BindGraphicState(BaseShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(BaseShader->GetPipelineLayout(), BaseShader->GetDescriptorSet(CurrentFrameRT));

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