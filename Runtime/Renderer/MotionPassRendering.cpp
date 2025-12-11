#include "DeferredShadingRenderer.h"

class UHMotionPassAsyncTask : public UHAsyncTask
{
public:
	void Init(UHDeferredShadingRenderer* InRenderer, const bool bInIsOpaque)
	{
		SceneRenderer = InRenderer;
		bIsOpaque = bInIsOpaque;
	}

	virtual void DoTask(const int32_t ThreadIdx) override
	{
		if (bIsOpaque)
		{
			SceneRenderer->MotionOpaqueTask(ThreadIdx);
		}
		else
		{
			SceneRenderer->MotionTranslucentTask(ThreadIdx);
		}
	}

private:
	UHDeferredShadingRenderer* SceneRenderer = nullptr;
	bool bIsOpaque = true;
};

void UHDeferredShadingRenderer::RenderMotionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderMotionPass", false);
	if (CurrentScene == nullptr)
	{
		return;
	}
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::MotionPass)], "MotionPass");

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Motion Pass");
	{
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GMotionVectorRT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneDepth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMixedDepth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();

		// copy opaque depth to translucent depth
		RenderBuilder.CopyTexture(GSceneDepth, GSceneMixedDepth);
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMixedDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();

		VkClearValue Clear = { 0,0,0,0 };
		RenderBuilder.BeginRenderPass(MotionCameraPassObj, RenderResolution, Clear);

		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// bind state
		UHGraphicState* State = MotionCameraShader->GetState();
		RenderBuilder.BindGraphicState(State);

		// bind sets
		RenderBuilder.BindDescriptorSet(MotionCameraShader->GetPipelineLayout(), MotionCameraShader->GetDescriptorSet(CurrentFrameRT));

		// doesn't need VB/IB for full screen quad
		RenderBuilder.BindVertexBuffer(nullptr);
		RenderBuilder.DrawFullScreenQuad();
		RenderBuilder.EndRenderPass();


		// -------------------- after motion camera pass is done, draw per-object motions, opaque first then the translucent -------------------- //
		// opaque motion will only render the dynamic objects (motion is dirty), static objects are already calculated in camera motion
		static std::vector<UHMotionPassAsyncTask> Tasks(NumParallelRenderSubmitters);
		{
			if (GraphicInterface->IsMeshShaderSupported())
			{
				RenderBuilder.BeginRenderPass(MotionOpaquePassObj, RenderResolution);

				// bindless table, they should only be bound once
				if (MotionMeshShaders.size() > 0 && SortedMeshShaderGroupIndex.size() > 0)
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
					RenderBuilder.BindDescriptorSet(MotionMeshShaders[SortedMeshShaderGroupIndex[0]]->GetPipelineLayout(), BindlessTableSets, GTextureTableSpace);
				}

				// dispatch mesh based on the MotionObjectMeshShaderData
				for (size_t Idx = 0; Idx < SortedMeshShaderGroupIndex.size(); Idx++)
				{
					const int32_t GroupIndex = SortedMeshShaderGroupIndex[Idx];
					const uint32_t VisibleMeshlets = static_cast<uint32_t>(MotionOpaqueMeshShaderData[GroupIndex].size());
					if (VisibleMeshlets == 0)
					{
						continue;
					}

					const UHMotionMeshShader* MotionMS = MotionMeshShaders[GroupIndex].get();
					if (!MotionMS->GetMaterialCache()->IsOpaque())
					{
						continue;
					}

					GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatching motion opaque pass " + MotionMS->GetMaterialCache()->GetName());

					RenderBuilder.BindGraphicState(MotionMS->GetState());
					RenderBuilder.BindDescriptorSet(MotionMS->GetPipelineLayout(), MotionMS->GetDescriptorSet(CurrentFrameRT));

					// Dispatch meshlets
					RenderBuilder.DispatchMesh(VisibleMeshlets, 1, 1);

					GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
				}
			}
			else
			{
				// begin for secondary cmd
				RenderBuilder.BeginRenderPass(MotionOpaquePassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
				for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
				{
					ThreadDrawCalls[I] = 0;
				}
#endif

				// init and wake all tasks
				for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
				{
					Tasks[I].Init(this, true);
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
				RenderBuilder.ExecuteBundles(MotionOpaqueParallelSubmitter);
			}
			RenderBuilder.EndRenderPass();
		}

		// translucent motion, however, needs to render all regardless if it's static or dynamic
		{
			// set scene mip as output
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMip, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneData, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

			if (GraphicInterface->IsMeshShaderSupported())
			{
				RenderBuilder.BeginRenderPass(MotionTranslucentPassObj, RenderResolution);

				// bindless table, they should only be bound once
				if (MotionMeshShaders.size() > 0 && SortedMeshShaderGroupIndex.size() > 0)
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
					RenderBuilder.BindDescriptorSet(MotionMeshShaders[SortedMeshShaderGroupIndex[0]]->GetPipelineLayout(), BindlessTableSets, GTextureTableSpace);
				}

				// dispatch mesh based on the MotionObjectMeshShaderData
				for (size_t Idx = 0; Idx < SortedMeshShaderGroupIndex.size(); Idx++)
				{
					const int32_t GroupIndex = SortedMeshShaderGroupIndex[Idx];
					const uint32_t VisibleMeshlets = static_cast<uint32_t>(MotionTranslucentMeshShaderData[GroupIndex].size());
					if (VisibleMeshlets == 0)
					{
						continue;
					}

					const UHMotionMeshShader* MotionMS = MotionMeshShaders[GroupIndex].get();
					if (MotionMS->GetMaterialCache()->IsOpaque())
					{
						continue;
					}

					GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatching motion translucent pass " + MotionMS->GetMaterialCache()->GetName());

					RenderBuilder.BindGraphicState(MotionMS->GetState());
					RenderBuilder.BindDescriptorSet(MotionMS->GetPipelineLayout(), MotionMS->GetDescriptorSet(CurrentFrameRT));

					// Dispatch meshlets
					RenderBuilder.DispatchMesh(VisibleMeshlets, 1, 1);

					GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
				}
			}
			else
			{

				RenderBuilder.BeginRenderPass(MotionTranslucentPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

#if WITH_EDITOR
				for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
				{
					ThreadDrawCalls[I] = 0;
				}
#endif

				// wake all worker threads
				for (int32_t I = 0; I < NumParallelRenderSubmitters; I++)
				{
					Tasks[I].Init(this, false);
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
				RenderBuilder.ExecuteBundles(MotionTranslucentParallelSubmitter);
			}

			RenderBuilder.EndRenderPass();

			// done rendering, transition depth to shader read
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMixedDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMip, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		}

		// depth/motion vector will be used in shader later, transition them
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GMotionVectorRT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::MotionOpaqueTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(MotionOpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumParallelRenderSubmitters) / NumParallelRenderSubmitters;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumParallelRenderSubmitters - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionOpaquePassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionOpaquePassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, MotionOpaqueParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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
	if (MotionOpaqueShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(MotionOpaqueShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		UHMeshRendererComponent* Renderer = MotionOpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();

		UHMesh* Mesh = Renderer->GetMesh();
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		const int32_t TriCount = Mesh->GetIndicesCount() / 3;

		const UHMotionObjectPassShader* MotionShader = MotionOpaqueShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(TriCount) + ")");

		const bool bOcclusionTest = RTParams.bEnableOcclusionQuery 
			&& TriCount >= RTParams.OcclusionThreshold && !Renderer->IsCameraInsideThisRenderer();
		if (bOcclusionTest)
		{
			RenderBuilder.BeginPredication(RendererIdx, GOcclusionResult[PrevFrame]->GetBuffer());
		}

		// bind pipelines
		RenderBuilder.BindGraphicState(MotionShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
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
#endif
}

void UHDeferredShadingRenderer::MotionTranslucentTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(TranslucentsToRender.size());
	const int32_t RendererCount = (MaxCount + NumParallelRenderSubmitters) / NumParallelRenderSubmitters;

	// to collect batch reversely
	const int32_t ReversedThreadIdx = NumParallelRenderSubmitters - ThreadIdx - 1;
	const int32_t StartIdx = std::min(RendererCount * ReversedThreadIdx, MaxCount);
	const int32_t EndIdx = (ReversedThreadIdx == NumParallelRenderSubmitters - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionTranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionTranslucentPassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, MotionTranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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
	if (MotionTranslucentShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(MotionTranslucentShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	// draw reversely since translucents are sort back-to-front
	// I want front-to-back order in motion pass for translucents
	const uint32_t PrevFrame = (CurrentFrameRT - 1) % GMaxFrameInFlight;
	for (int32_t I = EndIdx - 1; I >= StartIdx; I--)
	{
		UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();

		UHMesh* Mesh = Renderer->GetMesh();
		const int32_t RendererIdx = Renderer->GetBufferDataIndex();
		const int32_t TriCount = Mesh->GetIndicesCount() / 3;

		const UHMotionObjectPassShader* MotionShader = MotionTranslucentShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(TriCount) + ")");

		const bool bOcclusionTest = RTParams.bEnableOcclusionQuery 
			&& TriCount >= RTParams.OcclusionThreshold && !Renderer->IsCameraInsideThisRenderer();
		if (bOcclusionTest)
		{
			RenderBuilder.BeginPredication(RendererIdx, GOcclusionResult[PrevFrame]->GetBuffer());
		}

		// bind pipelines
		RenderBuilder.BindGraphicState(MotionShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
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
#endif
}