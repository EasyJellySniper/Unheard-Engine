#include "DeferredShadingRenderer.h"


void UHDeferredShadingRenderer::RefreshSkyLight(bool bNeedRecompile)
{
	GSkyLightCube = GetCurrentSkyCube();
	if (GSkyLightCube && !GSkyLightCube->IsBuilt())
	{
		VkCommandBuffer Cmd = GraphicInterface->BeginOneTimeCmd();
		UHRenderBuilder Builder(GraphicInterface, Cmd);
		GSkyLightCube->Build(GraphicInterface, Builder);
		GraphicInterface->EndOneTimeCmd(Cmd);
	}

	GraphicInterface->WaitGPU();
	SH9Shader->BindParameters();
	SkyPassShader->BindSkyCube();
	ReflectionPassShader->BindSkyCube();

#if WITH_EDITOR
	if (bNeedRecompile)
	{
		// recompiling all base and translucent shaders
		for (auto& BaseShader : BasePassShaders)
		{
			BaseShader.second->OnCompile();
		}

		for (auto& TransShader : TranslucentPassShaders)
		{
			TransShader.second->OnCompile();
		}
	}
#endif

	for (auto& TransShader : TranslucentPassShaders)
	{
		TransShader.second->BindSkyCube();
	}

	{
		bNeedGenerateSH9 = true;
	}

	if (RTParams.bEnableRayTracing)
	{
		RTReflectionShader->BindSkyCube();
	}
}

void UHDeferredShadingRenderer::GenerateSH9Pass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("GenerateSH9Pass", false);
	// generate SH9, this doesn't need to be called every frame
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::GenerateSH9)], "GenerateSH9");
	if (!RTParams.bEnableSkyLight || !RTParams.bNeedGenerateSH9)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Generate SH9");
	{
		RenderBuilder.BindComputeState(SH9Shader->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(SH9Shader->GetPipelineLayout(), SH9Shader->GetDescriptorSet(CurrentFrameRT));
		RenderBuilder.Dispatch(1, 1, 1);
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::RenderSkyPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RenderSkyPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::SkyPass)], "SkyPass");
	if (!RTParams.bEnableSkyLight)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Drawing Sky Pass");
	{
		RenderBuilder.BeginRenderPass(SkyboxPassObj, RenderResolution);

		RenderBuilder.SetViewport(RenderResolution);
		RenderBuilder.SetScissor(RenderResolution);

		// bind state
		RenderBuilder.BindGraphicState(SkyPassShader->GetState());

		// bind sets
		RenderBuilder.BindDescriptorSet(SkyPassShader->GetPipelineLayout(), SkyPassShader->GetDescriptorSet(CurrentFrameRT));

		// draw skybox renderer
		RenderBuilder.BindVertexBuffer(CubeMesh->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(CubeMesh);
		RenderBuilder.DrawIndexed(CubeMesh->GetIndicesCount());

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}