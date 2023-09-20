#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderMotionPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Motion Pass");
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::MotionPass]);

		// copy result to history velocity before rendering a new one
		GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		GraphBuilder.ResourceBarrier(PrevMotionVectorRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		GraphBuilder.CopyTexture(MotionVectorRT, PrevMotionVectorRT);
		GraphBuilder.ResourceBarrier(PrevMotionVectorRT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		VkClearValue Clear = { 0,0,0,0 };
		GraphBuilder.BeginRenderPass(MotionCameraPassObj.RenderPass, MotionCameraPassObj.FrameBuffer, RenderResolution, Clear);

		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);

		// bind state
		UHGraphicState* State = MotionCameraShader->GetState();
		GraphBuilder.BindGraphicState(State);

		// bind sets
		GraphBuilder.BindDescriptorSet(MotionCameraShader->GetPipelineLayout(), MotionCameraShader->GetDescriptorSet(CurrentFrameRT));

		// doesn't need VB/IB for full screen quad
		GraphBuilder.BindVertexBuffer(VK_NULL_HANDLE);
		GraphBuilder.DrawFullScreenQuad();
		GraphBuilder.EndRenderPass();


		// -------------------- after motion camera pass is done, draw per-object motions, opaque first then the translucent -------------------- //
		// opaque motion will only render the dynamic objects (motion is dirty), static objects are already calculated in camera motion
		{
			GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

			// begin for secondary cmd
			if (bParallelSubmissionRT)
			{
				GraphBuilder.BeginRenderPass(MotionOpaquePassObj.RenderPass, MotionOpaquePassObj.FrameBuffer, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			}
			else
			{
				GraphBuilder.BeginRenderPass(MotionOpaquePassObj.RenderPass, MotionOpaquePassObj.FrameBuffer, RenderResolution);
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
				ParallelTask = UHParallelTask::MotionOpaqueTask;
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
					GraphBuilder.DrawCalls += ThreadDrawCalls[I];
				}
#endif

				// execute all recorded batches
				GraphBuilder.ExecuteBundles(MotionOpaqueParallelSubmitter.WorkerBundles);
			}
			else
			{
				// bind texture table, they should only be bound once
				if (MotionOpaqueShaders.size() > 0)
				{
					std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
						, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
					GraphBuilder.BindDescriptorSet(MotionOpaqueShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
				}

				for (UHMeshRendererComponent* Renderer : OpaquesToRender)
				{
					const UHMaterial* Mat = Renderer->GetMaterial();
					UHMesh* Mesh = Renderer->GetMesh();

					// skip skybox mat and only draw dirty renderer
					if (Mat->IsSkybox() || !Renderer->IsMotionDirty(CurrentFrameRT))
					{
						continue;
					}

					const int32_t RendererIdx = Renderer->GetBufferDataIndex();
					if (MotionOpaqueShaders.find(RendererIdx) == MotionOpaqueShaders.end())
					{
						// unlikely to happen, but printing a message for debug
						UHE_LOG(L"[MotionObjectPass] Can't find motion object pass shader for material: \n");
						continue;
					}

					const UHMotionObjectPassShader* MotionShader = MotionOpaqueShaders[RendererIdx].get();

					GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
						std::to_string(Mesh->GetIndicesCount() / 3) + ")");

					// bind pipelines
					GraphBuilder.BindGraphicState(MotionShader->GetState());
					GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
					GraphBuilder.BindIndexBuffer(Mesh);
					GraphBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

					// draw call
					GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());
					GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
					Renderer->SetMotionDirty(false, CurrentFrameRT);
				}
			}

			GraphBuilder.EndRenderPass();
		}

		// translucent motion, however, needs to render all regardless if it's static or dynamic
		{
			// copy opaque depth to translucent depth RT
			GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			GraphBuilder.ResourceBarrier(SceneTranslucentDepth, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			GraphBuilder.CopyTexture(SceneDepth, SceneTranslucentDepth);
			GraphBuilder.ResourceBarrier(SceneTranslucentDepth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

			// clear translucent vertex normal before writing to it
			GraphBuilder.ResourceBarrier(SceneTranslucentVertexNormal, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			GraphBuilder.ClearRenderTexture(SceneTranslucentVertexNormal);
			GraphBuilder.ResourceBarrier(SceneTranslucentVertexNormal, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			if (bParallelSubmissionRT)
			{
				GraphBuilder.BeginRenderPass(MotionTranslucentPassObj.RenderPass, MotionTranslucentPassObj.FrameBuffer, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			}
			else
			{
				GraphBuilder.BeginRenderPass(MotionTranslucentPassObj.RenderPass, MotionTranslucentPassObj.FrameBuffer, RenderResolution);
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
				ParallelTask = UHParallelTask::MotionTranslucentTask;
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
					GraphBuilder.DrawCalls += ThreadDrawCalls[I];
				}
#endif

				// execute all recorded batches reversely, as translucents are sorted back-to-front
				std::reverse(MotionTranslucentParallelSubmitter.WorkerBundles.begin(), MotionTranslucentParallelSubmitter.WorkerBundles.end());
				GraphBuilder.ExecuteBundles(MotionTranslucentParallelSubmitter.WorkerBundles);
			}
			else
			{
				// bind texture table, they should only be bound once
				if (MotionTranslucentShaders.size() > 0)
				{
					std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
						, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
					GraphBuilder.BindDescriptorSet(MotionTranslucentShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
				}

				for (int32_t Idx = static_cast<int32_t>(TranslucentsToRender.size()) - 1; Idx >= 0; Idx--)
				{
					const UHMeshRendererComponent* Renderer = TranslucentsToRender[Idx];
					const UHMaterial* Mat = Renderer->GetMaterial();
					UHMesh* Mesh = Renderer->GetMesh();

					// skip skybox mat
					if (Mat->IsSkybox())
					{
						continue;
					}

					const int32_t RendererIdx = Renderer->GetBufferDataIndex();
					if (MotionTranslucentShaders.find(RendererIdx) == MotionTranslucentShaders.end())
					{
						// unlikely to happen, but printing a message for debug
						UHE_LOG(L"[MotionObjectPass] Can't find motion object pass shader for material: \n");
						continue;
					}

					const UHMotionObjectPassShader* MotionShader = MotionTranslucentShaders[RendererIdx].get();

					GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
						std::to_string(Mesh->GetIndicesCount() / 3) + ")");

					// bind pipelines
					GraphBuilder.BindGraphicState(MotionShader->GetState());
					GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
					GraphBuilder.BindIndexBuffer(Mesh);
					GraphBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

					// draw call
					GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());
					GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
				}
			}

			GraphBuilder.EndRenderPass();

			// done rendering, transition depth to shader read
			GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			GraphBuilder.ResourceBarrier(SceneTranslucentDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			GraphBuilder.ResourceBarrier(SceneTranslucentVertexNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		// depth/motion vector will be used in shader later, transition them
		GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::MotionOpaqueTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(OpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionOpaquePassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionOpaquePassObj.FrameBuffer;

	UHGraphicBuilder GraphBuilder(GraphicInterface, MotionOpaqueParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	GraphBuilder.BeginCommandBuffer(&InheritanceInfo);

	// silence the false positive error regarding vp and scissor
	if (GraphicInterface->IsDebugLayerEnabled())
	{
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);
	}

	// bind texture table, they should only be bound once
	if (MotionOpaqueShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		GraphBuilder.BindDescriptorSet(MotionOpaqueShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		UHMeshRendererComponent* Renderer = OpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->IsSkybox() || !Renderer->IsMotionDirty(CurrentFrameRT))
		{
			continue;
		}

		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHMotionObjectPassShader* MotionShader = MotionOpaqueShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(MotionShader->GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
		Renderer->SetMotionDirty(false, CurrentFrameRT);
	}

	GraphBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += GraphBuilder.DrawCalls;
#endif
}

void UHDeferredShadingRenderer::MotionTranslucentTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(TranslucentsToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionTranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionTranslucentPassObj.FrameBuffer;

	UHGraphicBuilder GraphBuilder(GraphicInterface, MotionTranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	GraphBuilder.BeginCommandBuffer(&InheritanceInfo);

	// silence the false positive error regarding vp and scissor
	if (GraphicInterface->IsDebugLayerEnabled())
	{
		GraphBuilder.SetViewport(RenderResolution);
		GraphBuilder.SetScissor(RenderResolution);
	}

	// bind texture table, they should only be bound once
	if (MotionTranslucentShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		GraphBuilder.BindDescriptorSet(MotionTranslucentShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	// draw reversely since translucents are sort back-to-front
	for (int32_t I = EndIdx - 1; I >= StartIdx; I--)
	{
		UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->IsSkybox())
		{
			continue;
		}

		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHMotionObjectPassShader* MotionShader = MotionTranslucentShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(MotionShader->GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());
		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	}

	GraphBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += GraphBuilder.DrawCalls;
#endif
}