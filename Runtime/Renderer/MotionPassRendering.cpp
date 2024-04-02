#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderMotionPass(UHRenderBuilder& RenderBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::MotionPass]);

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Motion Pass");
	{
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GMotionVectorRT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneDepth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneTranslucentDepth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();

		// copy opaque depth to translucent depth
		RenderBuilder.CopyTexture(GSceneDepth, GSceneTranslucentDepth);
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneTranslucentDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
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
		{
			// begin for secondary cmd
			RenderBuilder.BeginRenderPass(MotionOpaquePassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

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
				RenderBuilder.DrawCalls += ThreadDrawCalls[I];
			}
#endif

			// execute all recorded batches
			RenderBuilder.ExecuteBundles(MotionOpaqueParallelSubmitter.WorkerBundles);
			RenderBuilder.EndRenderPass();
		}

		// translucent motion, however, needs to render all regardless if it's static or dynamic
		{
			// set vertex normal/mip as color output, translucent vertex normal/mip will be output to it too
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneVertexNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMip, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

			// clear translucent bump & roughness buffer and transition to color output
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentBump, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentSmoothness, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
			RenderBuilder.FlushResourceBarrier();
			RenderBuilder.ClearRenderTexture(GTranslucentBump);
			RenderBuilder.ClearRenderTexture(GTranslucentSmoothness);

			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentBump, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentSmoothness, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			RenderBuilder.FlushResourceBarrier();

			RenderBuilder.BeginRenderPass(MotionTranslucentPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

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
				RenderBuilder.DrawCalls += ThreadDrawCalls[I];
			}
#endif

			// execute all recorded batches
			RenderBuilder.ExecuteBundles(MotionTranslucentParallelSubmitter.WorkerBundles);
			RenderBuilder.EndRenderPass();

			// done rendering, transition depth to shader read
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneTranslucentDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneVertexNormal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneMip, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentBump, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GTranslucentSmoothness, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
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
	const int32_t MaxCount = static_cast<int32_t>(OpaquesToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionOpaquePassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionOpaquePassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, MotionOpaqueParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
	RenderBuilder.BeginCommandBuffer(&InheritanceInfo);
	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	if (GraphicInterface->IsAMDIntegratedGPU() && ThreadIdx == 0)
	{
		// draw camera motion again for a bug on AMD integreated GPUs
		UHGraphicState* State = MotionCameraWorkaroundShader->GetState();
		RenderBuilder.BindGraphicState(State);
		RenderBuilder.BindDescriptorSet(MotionCameraWorkaroundShader->GetPipelineLayout(), MotionCameraWorkaroundShader->GetDescriptorSet(CurrentFrameRT));
		RenderBuilder.BindVertexBuffer(nullptr);
		RenderBuilder.DrawFullScreenQuad();
	}

	// bind texture table, they should only be bound once
	if (MotionOpaqueShaders.size() > 0)
	{
		std::vector<VkDescriptorSet> TextureTableSets = { TextureTable->GetDescriptorSet(CurrentFrameRT)
			, SamplerTable->GetDescriptorSet(CurrentFrameRT) };
		RenderBuilder.BindDescriptorSet(MotionOpaqueShaders.begin()->second->GetPipelineLayout(), TextureTableSets, GTextureTableSpace);
	}

	for (int32_t I = StartIdx; I < EndIdx; I++)
	{
		UHMeshRendererComponent* Renderer = OpaquesToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->GetMaterialUsages().bIsSkybox || (!Renderer->IsMotionDirty(CurrentFrameRT)))
		{
			continue;
		}

		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHMotionObjectPassShader* MotionShader = MotionOpaqueShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		RenderBuilder.BindGraphicState(MotionShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
		Renderer->SetMotionDirty(false, CurrentFrameRT);
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
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;

	// to collect batch reversely
	const int32_t ReversedThreadIdx = NumWorkerThreads - ThreadIdx - 1;
	const int32_t StartIdx = std::min(RendererCount * ReversedThreadIdx, MaxCount);
	const int32_t EndIdx = (ReversedThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = MotionTranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = MotionTranslucentPassObj.FrameBuffer;

	UHRenderBuilder RenderBuilder(GraphicInterface, MotionTranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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
	for (int32_t I = EndIdx - 1; I >= StartIdx; I--)
	{
		UHMeshRendererComponent* Renderer = TranslucentsToRender[I];
		const UHMaterial* Mat = Renderer->GetMaterial();
		if (Mat->GetMaterialUsages().bIsSkybox)
		{
			continue;
		}

		UHMesh* Mesh = Renderer->GetMesh();
		int32_t RendererIdx = Renderer->GetBufferDataIndex();

		const UHMotionObjectPassShader* MotionShader = MotionTranslucentShaders[RendererIdx].get();

		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		RenderBuilder.BindGraphicState(MotionShader->GetState());
		RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(Mesh);
		RenderBuilder.BindDescriptorSet(MotionShader->GetPipelineLayout(), MotionShader->GetDescriptorSet(CurrentFrameRT));

		// draw call
		RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());
		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
#endif
}