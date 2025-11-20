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
	if (RTParams.bNeedRefraction)
	{
		// Blur the opaque scene
		ScreenshotForRefraction("Opaque Scene Blur", RenderBuilder);
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("DispatchReflectionPass", false);
	// pass to draw reflection, this pass will mainly apply reflection to opaque scene
	if (CurrentScene == nullptr)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::ReflectionPass)], "ReflectionPass");
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Render Reflection Pass");
	{
		UHComputeState* State = ReflectionPassShader->GetComputeState();
		RenderBuilder.BindComputeState(State);
		RenderBuilder.BindDescriptorSetCompute(ReflectionPassShader->GetPipelineLayout(), ReflectionPassShader->GetDescriptorSet(CurrentFrameRT));

		// dispatch
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y), 1);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}