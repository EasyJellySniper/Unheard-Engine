#include "DeferredShadingRenderer.h"

#if WITH_EDITOR
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
	SkyPassShader->BindImage(GSkyLightCube, 1);

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

	for (auto& BaseShader : BasePassShaders)
	{
		BaseShader.second->BindImage(GSkyLightCube, 6);
	}

	for (auto& TransShader : TranslucentPassShaders)
	{
		TransShader.second->BindImage(GSkyLightCube, 14);
	}

	{
		std::unique_lock<std::mutex> Lock(RenderThread->GetThreadMutex());
		bNeedGenerateSH9RT = true;
	}
}
#endif

void UHDeferredShadingRenderer::GenerateSH9Pass(UHRenderBuilder& RenderBuilder)
{
	// generate SH9, this doesn't need to be called every frame
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::GenerateSH9]);
	if (!bIsSkyLightEnabledRT || !bNeedGenerateSH9RT)
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
	bNeedGenerateSH9RT = false;
}

void UHDeferredShadingRenderer::RenderSkyPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::SkyPass]);
	RenderBuilder.ResourceBarrier(GSceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	if (!bIsSkyLightEnabledRT)
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
		RenderBuilder.BindVertexBuffer(SkyMeshRT->GetPositionBuffer()->GetBuffer());
		RenderBuilder.BindIndexBuffer(SkyMeshRT);
		RenderBuilder.DrawIndexed(SkyMeshRT->GetIndicesCount());

		RenderBuilder.EndRenderPass();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}