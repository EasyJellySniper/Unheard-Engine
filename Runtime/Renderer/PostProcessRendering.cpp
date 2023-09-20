#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderEffect(UHShaderClass* InShader, UHGraphicBuilder& GraphBuilder, int32_t& PostProcessIdx, std::string InName)
{
	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Render " + InName);

	GraphBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.BeginRenderPass(PostProcessPassObj[PostProcessIdx].RenderPass, PostProcessPassObj[PostProcessIdx].FrameBuffer, RenderResolution);

	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = InShader->GetState();
	GraphBuilder.BindGraphicState(State);

	// bind sets
	GraphBuilder.BindDescriptorSet(InShader->GetPipelineLayout(), InShader->GetDescriptorSet(CurrentFrameRT));

	// doesn't need VB/IB for full screen quad
	GraphBuilder.DrawFullScreenQuad();

	GraphBuilder.EndRenderPass();
	GraphBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	PostProcessIdx = 1 - PostProcessIdx;
}

void UHDeferredShadingRenderer::DispatchEffect(UHShaderClass* InShader, UHGraphicBuilder& GraphBuilder, int32_t& PostProcessIdx, std::string InName)
{
	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Dispatch " + InName);

	GraphBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(PostProcessResults[PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	// bind compute state
	UHGraphicState* State = InShader->GetComputeState();
	GraphBuilder.BindComputeState(State);

	// bind sets
	GraphBuilder.BindDescriptorSetCompute(InShader->GetPipelineLayout(), InShader->GetDescriptorSet(CurrentFrameRT));

	// dispatch compute
	GraphBuilder.Dispatch((RenderResolution.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RenderResolution.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

	GraphBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	GraphBuilder.ResourceBarrier(PostProcessResults[PostProcessIdx], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
	PostProcessIdx = 1 - PostProcessIdx;
}

void UHDeferredShadingRenderer::RenderPostProcessing(UHGraphicBuilder& GraphBuilder)
{
	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Postprocessing Passes");
	// post process RT starts from undefined, transition it first
	GraphBuilder.ResourceBarrier(PostProcessRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	PostProcessResults[0] = PostProcessRT;
	PostProcessResults[1] = SceneResult;

	// this index will toggle between 0 and 1 during the post processing
	int32_t CurrentPostProcessRTIndex = 0;

	// -------------------------- Tone Mapping --------------------------//
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::ToneMappingPass]);
		ToneMapShader->BindImage(PostProcessResults[1 - CurrentPostProcessRTIndex], 0, CurrentFrameRT);
		RenderEffect(ToneMapShader.get(), GraphBuilder, CurrentPostProcessRTIndex, "Tone mapping");
	}

	// -------------------------- Temporal AA --------------------------//
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::TemporalAAPass]);
		if (ConfigInterface->RenderingSetting().bTemporalAA)
		{
			GraphBuilder.ResourceBarrier(PreviousSceneResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			if (!bIsTemporalReset)
			{
				// only render it when it's not resetting
				TemporalAAShader->BindImage(PostProcessResults[CurrentPostProcessRTIndex], 1, CurrentFrameRT, true);
				TemporalAAShader->BindImage(PostProcessResults[1 - CurrentPostProcessRTIndex], 2, CurrentFrameRT);
				DispatchEffect(TemporalAAShader.get(), GraphBuilder, CurrentPostProcessRTIndex, "Temporal AA");
			}

			// copy to TAA history
			GraphBuilder.ResourceBarrier(PreviousSceneResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			GraphBuilder.ResourceBarrier(PostProcessResults[1 - CurrentPostProcessRTIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			GraphBuilder.CopyTexture(PostProcessResults[1 - CurrentPostProcessRTIndex], PreviousSceneResult);
			GraphBuilder.ResourceBarrier(PostProcessResults[1 - CurrentPostProcessRTIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			bIsTemporalReset = false;
		}
	}

#if WITH_EDITOR
	if (DebugViewIndex > 0)
	{
		RenderEffect(DebugViewShader.get(), GraphBuilder, CurrentPostProcessRTIndex, "Debug View");
	}

	RenderComponentBounds(GraphBuilder, 1 - CurrentPostProcessRTIndex);
#endif

	// set post process result idx in the end
	PostProcessResultIdx = 1 - CurrentPostProcessRTIndex;

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}

uint32_t UHDeferredShadingRenderer::RenderSceneToSwapChain(UHGraphicBuilder& GraphBuilder)
{
	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Scene to SwapChain Pass");

	uint32_t ImageIndex;
	{
		UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::PresentToSwapChain]);

		VkRenderPass SwapChainRenderPass = GraphicInterface->GetSwapChainRenderPass();
		VkFramebuffer SwapChainBuffer = GraphBuilder.GetCurrentSwapChainBuffer(SceneRenderQueue.WaitingSemaphores[CurrentFrameRT], ImageIndex);
		UHRenderTexture* SwapChainRT = GraphicInterface->GetSwapChainRT(ImageIndex);
		VkExtent2D SwapChainExtent = GraphicInterface->GetSwapChainExtent();

		// this will transition to TRANSDER_DST
		GraphBuilder.BeginRenderPass(SwapChainRenderPass, SwapChainBuffer, SwapChainExtent);
		GraphBuilder.EndRenderPass();

		// transfer scene result and blit it, the scene result comes after post processing, it will be SceneResult or PostProcessRT
		GraphBuilder.ResourceBarrier(PostProcessResults[PostProcessResultIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		// blit with the same aspect ratio as render resolution regardless of the swap chain size
		VkExtent2D ConstraintedOffset;
		VkExtent2D ConstraintedExtent;
		ConstraintedExtent.width = SwapChainExtent.width;
		ConstraintedExtent.height = SwapChainExtent.width * RenderResolution.height / RenderResolution.width;
		ConstraintedOffset.width = 0;
		ConstraintedOffset.height = (SwapChainExtent.height - ConstraintedExtent.height) / 2;

		if (ConstraintedExtent.height > SwapChainExtent.height)
		{
			ConstraintedExtent.height = SwapChainExtent.height;
			ConstraintedExtent.width = SwapChainExtent.height * RenderResolution.width / RenderResolution.height;
			ConstraintedOffset.height = 0;
			ConstraintedOffset.width = (SwapChainExtent.width - ConstraintedExtent.width) / 2;
		}

		if (!ConfigInterface->PresentationSetting().bFullScreen)
		{
			GraphBuilder.Blit(PostProcessResults[PostProcessResultIdx], SwapChainRT, PostProcessResults[PostProcessResultIdx]->GetExtent(), ConstraintedExtent, ConstraintedOffset);
		}
		else
		{
			GraphBuilder.Blit(PostProcessResults[PostProcessResultIdx], SwapChainRT);
		}

		// end blit and transition swapchain to PRESENT_SRC_KHR
		GraphBuilder.ResourceBarrier(SwapChainRT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());

	return ImageIndex;
}

#if WITH_EDITOR
void UHDeferredShadingRenderer::RenderComponentBounds(UHGraphicBuilder& GraphBuilder, const int32_t PostProcessIdx)
{
	// render the bound of selected component
	const UHComponent* Comp = nullptr;
	if (CurrentScene)
	{
		Comp = CurrentScene->GetCurrentSelectedComponent();
	}

	if (Comp == nullptr)
	{
		return;
	}

	UHDebugBoundConstant BoundConstant = Comp->GetDebugBoundConst();
	if (BoundConstant.BoundType == DebugNone)
	{
		return;
	}

	DebugBoundData[CurrentFrameRT]->UploadAllData(&BoundConstant);
	DebugBoundShader->BindConstant(DebugBoundData[CurrentFrameRT], 1, CurrentFrameRT);

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Draw Component Bounds");
	GraphBuilder.BeginRenderPass(PostProcessPassObj[PostProcessIdx].RenderPass, PostProcessPassObj[PostProcessIdx].FrameBuffer, RenderResolution);

	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = DebugBoundShader->GetState();
	GraphBuilder.BindGraphicState(State);

	// bind sets
	GraphBuilder.BindDescriptorSet(DebugBoundShader->GetPipelineLayout(), DebugBoundShader->GetDescriptorSet(CurrentFrameRT));

	vkCmdSetLineWidth(GraphBuilder.GetCmdList(), 2.0f);

	switch (BoundConstant.BoundType)
	{
	case DebugBox:
		// draw 24 points for bounding box, shader will setup the box points
		GraphBuilder.DrawVertex(24);
		break;

	case DebugSphere:
		GraphBuilder.DrawVertex(360);
		break;

	default:
		break;
	}

	GraphBuilder.EndRenderPass();
	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}
#endif