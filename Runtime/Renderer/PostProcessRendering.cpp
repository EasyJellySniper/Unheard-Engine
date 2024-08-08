#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName)
{
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Render " + InName);

	RenderBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	RenderBuilder.BeginRenderPass(PostProcessPassObj[PostProcessIdx], RenderResolution);

	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = InShader->GetState();
	RenderBuilder.BindGraphicState(State);

	// bind sets
	RenderBuilder.BindDescriptorSet(InShader->GetPipelineLayout(), InShader->GetDescriptorSet(CurrentFrameRT));

	// doesn't need VB/IB for full screen quad
	RenderBuilder.DrawFullScreenQuad();

	RenderBuilder.EndRenderPass();
	RenderBuilder.ResourceBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	PostProcessIdx = 1 - PostProcessIdx;
}

void UHDeferredShadingRenderer::Dispatch2DEffect(UHShaderClass* InShader, UHRenderBuilder& RenderBuilder, int32_t& PostProcessIdx, std::string InName)
{
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch " + InName);

	RenderBuilder.PushResourceBarrier(UHImageBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(PostProcessResults[PostProcessIdx], VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.FlushResourceBarrier();

	// bind compute state
	UHGraphicState* State = InShader->GetComputeState();
	RenderBuilder.BindComputeState(State);

	// bind sets
	RenderBuilder.BindDescriptorSetCompute(InShader->GetPipelineLayout(), InShader->GetDescriptorSet(CurrentFrameRT));

	// dispatch compute
	RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
		, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y), 1);

	RenderBuilder.PushResourceBarrier(UHImageBarrier(PostProcessResults[1 - PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(PostProcessResults[PostProcessIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	PostProcessIdx = 1 - PostProcessIdx;
}

void UHDeferredShadingRenderer::RenderPostProcessing(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderPostProcessing", false);
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Postprocessing Passes");
	// post process RT starts from undefined, transition it first
	RenderBuilder.ResourceBarrier(GPostProcessRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	PostProcessResults[0] = GPostProcessRT;
	PostProcessResults[1] = GSceneResult;

	// this index will toggle between 0 and 1 during the post processing
	int32_t CurrentPostProcessRTIndex = 0;

	// -------------------------- Tone Mapping --------------------------//
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::ToneMappingPass)], "ToneMappingPass");
		ToneMapShader->BindInputImage(PostProcessResults[1 - CurrentPostProcessRTIndex], CurrentFrameRT);
		RenderEffect(ToneMapShader.get(), RenderBuilder, CurrentPostProcessRTIndex, "Tone mapping");
	}
	
	// -------------------------- Temporal AA --------------------------//
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::TemporalAAPass)], "TemporalAAPass");
		if (ConfigInterface->RenderingSetting().bTemporalAA)
		{
			RenderBuilder.ResourceBarrier(GPreviousSceneResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			if (!bIsTemporalReset)
			{
				// only render it when it's not resetting
				TemporalAAShader->BindImage(PostProcessResults[CurrentPostProcessRTIndex], 1, CurrentFrameRT, true);
				TemporalAAShader->BindImage(PostProcessResults[1 - CurrentPostProcessRTIndex], 2, CurrentFrameRT);
				Dispatch2DEffect(TemporalAAShader.get(), RenderBuilder, CurrentPostProcessRTIndex, "Temporal AA");
			}

			bIsTemporalReset = false;
		}
	}

	// -------------------------- History Result Passes --------------------------//
	{
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::HistoryCopyingPass)], "HistoryCopyingPass");
		GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "History Result Copy");

		// blit to scene history
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GPreviousSceneResult, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(PostProcessResults[1 - CurrentPostProcessRTIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();
		RenderBuilder.Blit(PostProcessResults[1 - CurrentPostProcessRTIndex], GPreviousSceneResult);

		RenderBuilder.ResourceBarrier(PostProcessResults[1 - CurrentPostProcessRTIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	}

#if WITH_EDITOR
	if (DebugViewIndex > 0)
	{
		if (bDrawDebugViewRT)
		{
			RenderEffect(DebugViewShader.get(), RenderBuilder, CurrentPostProcessRTIndex, "Debug View");
		}
	}

	RenderComponentBounds(RenderBuilder, 1 - CurrentPostProcessRTIndex);
#endif

	// set post process result idx in the end
	PostProcessResultIdx = 1 - CurrentPostProcessRTIndex;

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

uint32_t UHDeferredShadingRenderer::RenderSceneToSwapChain(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderSceneToSwapChain", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::PresentToSwapChain)], "PresentToSwapChain");
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Scene to SwapChain Pass");

	uint32_t ImageIndex;
	{
		VkRenderPass SwapChainRenderPass = GraphicInterface->GetSwapChainRenderPass();
		VkFramebuffer SwapChainBuffer = RenderBuilder.GetCurrentSwapChainBuffer(SceneRenderQueue.WaitingSemaphores[CurrentFrameRT], ImageIndex);
		UHRenderTexture* SwapChainRT = GraphicInterface->GetSwapChainRT(ImageIndex);
		VkExtent2D SwapChainExtent = GraphicInterface->GetSwapChainExtent();

	#if WITH_EDITOR
		// consider editor delta
		SwapChainExtent.width -= EditorWidthDelta;
		SwapChainExtent.height -= EditorHeightDelta;
	#endif

		// manually clear swap chain
		RenderBuilder.ResourceBarrier(SwapChainRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		RenderBuilder.ClearRenderTexture(SwapChainRT);

		if (bIsRenderingEnabledRT)
		{
			// transfer scene result and blit it, the scene result comes after post processing, it will be SceneResult or PostProcessRT
			RenderBuilder.ResourceBarrier(PostProcessResults[PostProcessResultIdx], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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
				RenderBuilder.Blit(PostProcessResults[PostProcessResultIdx], SwapChainRT, PostProcessResults[PostProcessResultIdx]->GetExtent(), ConstraintedExtent, ConstraintedOffset);
			}
			else
			{
				RenderBuilder.Blit(PostProcessResults[PostProcessResultIdx], SwapChainRT);
			}
		}

#if WITH_EDITOR
		// editor ImGui rendering
		SwapChainExtent = GraphicInterface->GetSwapChainExtent();
		UHRenderPassObject Obj;
		Obj.RenderPass = SwapChainRenderPass;
		Obj.FrameBuffer = SwapChainBuffer;
		RenderBuilder.BeginRenderPass(Obj, SwapChainExtent);
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), RenderBuilder.GetCmdList(), GraphicInterface->GetImGuiPipeline());
		RenderBuilder.EndRenderPass();
#endif

		// end blit and transition swapchain to PRESENT_SRC_KHR
		RenderBuilder.ResourceBarrier(SwapChainRT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());

	return ImageIndex;
}

#if WITH_EDITOR
void UHDeferredShadingRenderer::RenderComponentBounds(UHRenderBuilder& RenderBuilder, const int32_t PostProcessIdx)
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
	if (BoundConstant.BoundType == UHDebugBoundType::DebugNone)
	{
		return;
	}

	DebugBoundShader->GetDebugBoundData(CurrentFrameRT)->UploadAllData(&BoundConstant);

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Draw Component Bounds");
	RenderBuilder.BeginRenderPass(PostProcessPassObj[PostProcessIdx], RenderResolution);

	RenderBuilder.SetViewport(RenderResolution);
	RenderBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = DebugBoundShader->GetState();
	RenderBuilder.BindGraphicState(State);

	// bind sets
	RenderBuilder.BindDescriptorSet(DebugBoundShader->GetPipelineLayout(), DebugBoundShader->GetDescriptorSet(CurrentFrameRT));

	vkCmdSetLineWidth(RenderBuilder.GetCmdList(), 2.0f);

	switch (BoundConstant.BoundType)
	{
	case UHDebugBoundType::DebugBox:
		// draw 24 points for bounding box, shader will setup the box points
		RenderBuilder.DrawVertex(24);
		break;

	case UHDebugBoundType::DebugSphere:
		// draw 360 points for bounding sphere, see DebugBoundShader.hlsl that how circles are drawn
		RenderBuilder.DrawVertex(360);
		break;

	case UHDebugBoundType::DebugCone:
		// draw 16 points for bounding cone, see DebugBoundShader.hlsl that how a cone is drawn
		RenderBuilder.DrawVertex(16);
		break;

	default:
		break;
	}

	RenderBuilder.EndRenderPass();
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}
#endif