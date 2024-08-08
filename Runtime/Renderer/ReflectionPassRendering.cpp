#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::PreReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("PreReflectionPass", false);
	if (CurrentScene == nullptr)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::PreReflectionPass)], "PreReflectionPass");
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Pre reflection Pass");

	// opaque scene capture before applying reflection
	if (bHasRefractionMaterialRT)
	{
		// Blur the opaque scene
		ScreenshotForRefraction("Opaque Scene Blur", RenderBuilder, RefractionGaussianConsts);
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DrawReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("DrawReflectionPass", false);
	// pass to draw reflection, this pass will mainly apply reflection to opaque scene
	if (CurrentScene == nullptr)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::ReflectionPass)], "ReflectionPass");
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Reflection Pass");
	{
		RenderBuilder.ResourceBarrier(GSceneResult, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

		UHComputeState* State = ReflectionPassShader->GetComputeState();
		RenderBuilder.BindComputeState(State);
		RenderBuilder.BindDescriptorSetCompute(ReflectionPassShader->GetPipelineLayout(), ReflectionPassShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y), 1);

		RenderBuilder.ResourceBarrier(GSceneResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}