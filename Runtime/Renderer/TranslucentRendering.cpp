#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::ScreenshotForRefraction(std::string PassName, UHRenderBuilder& RenderBuilder, UHGaussianFilterConstants Constants)
{
	// blit the scene result to opaque scene result
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GQuarterBlurredScene, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneResult, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();

	RenderBuilder.Blit(GSceneResult, GOpaqueSceneResult);
	RenderBuilder.Blit(GSceneResult, GQuarterBlurredScene);

	DispatchGaussianFilter(RenderBuilder, PassName, GQuarterBlurredScene, GQuarterBlurredScene, Constants);

	// final transition
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GOpaqueSceneResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GQuarterBlurredScene, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GSceneResult, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
}

void UHDeferredShadingRenderer::RenderTranslucentPass(UHRenderBuilder& RenderBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::TranslucentPass)]);

	// this pass doesn't need a RT clear, will draw on scene result directly
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Translucent Pass");
	{
		// setup viewport and scissor
		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);
		// bind depth
		RenderBuilder.ResourceBarrier(GSceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		// Draw the translucent background
		{
			RenderBuilder.BeginRenderPass(TranslucentPassObj, RenderResolution, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
#if WITH_EDITOR
			for (int32_t I = 0; I < NumWorkerThreads; I++)
			{
				ThreadDrawCalls[I] = 0;
				ThreadOccludedCalls[I] = 0;
			}
#endif

			// parallel pass
			ParallelTask = UHParallelTask::TranslucentBgPassTask;
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
				RenderBuilder.OccludedCalls += ThreadOccludedCalls[I];
			}
#endif
			// execute all recorded batches
			RenderBuilder.ExecuteBundles(TranslucentParallelSubmitter.WorkerBundles);
			RenderBuilder.EndRenderPass();
		}

		RenderBuilder.ResourceBarrier(GSceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::TranslucentPassTask(int32_t ThreadIdx)
{
	// simply separate buffer recording into N threads
	const int32_t MaxCount = static_cast<int32_t>(TranslucentsToRender.size());
	const int32_t RendererCount = (MaxCount + NumWorkerThreads) / NumWorkerThreads;
	const int32_t StartIdx = std::min(RendererCount * ThreadIdx, MaxCount);
	const int32_t EndIdx = (ThreadIdx == NumWorkerThreads - 1) ? MaxCount : std::min(StartIdx + RendererCount, MaxCount);

	VkCommandBufferInheritanceInfo InheritanceInfo{};
	InheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	InheritanceInfo.renderPass = TranslucentPassObj.RenderPass;
	InheritanceInfo.framebuffer = TranslucentPassObj.FrameBuffer;
	InheritanceInfo.occlusionQueryEnable = bEnableHWOcclusionRT;

	UHRenderBuilder RenderBuilder(GraphicInterface, TranslucentParallelSubmitter.WorkerCommandBuffers[ThreadIdx * GMaxFrameInFlight + CurrentFrameRT]);
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

		if (!bEnableHWOcclusionRT || TriCount < OcclusionThresholdRT || (bEnableHWOcclusionRT && OcclusionResult[PrevFrame][RendererIdx] > 0))
		{
			// draw visible complex mesh or small mesh
			const UHTranslucentPassShader* TranslucentShader = TranslucentPassShaders[RendererIdx].get();
			RenderBuilder.BindGraphicState(TranslucentShader->GetState());
			RenderBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
			RenderBuilder.BindIndexBuffer(Mesh);
			RenderBuilder.BindDescriptorSet(TranslucentShader->GetPipelineLayout(), TranslucentShader->GetDescriptorSet(CurrentFrameRT));

			RenderBuilder.DrawIndexed(Mesh->GetIndicesCount());
		}

		if (bEnableHWOcclusionRT && TriCount >= OcclusionThresholdRT)
		{
			// draw bounding box and test for big meshes regardless if it's occluded
			RenderBuilder.BeginOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);

			const UHTranslucentPassShader* BaseShader = TranslucentOcclusionShaders[RendererIdx].get();
			RenderBuilder.BindGraphicState(BaseShader->GetOcclusionState());
			RenderBuilder.BindVertexBuffer(SkyMeshRT->GetPositionBuffer()->GetBuffer());
			RenderBuilder.BindIndexBuffer(SkyMeshRT);
			RenderBuilder.BindDescriptorSet(BaseShader->GetPipelineLayout(), BaseShader->GetDescriptorSet(CurrentFrameRT));

			RenderBuilder.DrawIndexed(SkyMeshRT->GetIndicesCount(), true);
			RenderBuilder.EndOcclusionQuery(OcclusionQuery[CurrentFrameRT], RendererIdx);
		}

		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

	RenderBuilder.EndCommandBuffer();
#if WITH_EDITOR
	ThreadDrawCalls[ThreadIdx] += RenderBuilder.DrawCalls;
	ThreadOccludedCalls[ThreadIdx] += RenderBuilder.OccludedCalls;
#endif
}