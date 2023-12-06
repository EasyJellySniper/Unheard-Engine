#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::ResizeRayTracingBuffers(bool bInitOnly)
{
	if (GraphicInterface->IsRayTracingEnabled())
	{
		if (!bInitOnly)
		{
			GraphicInterface->WaitGPU();
			GraphicInterface->RequestReleaseRT(GRTShadowResult);
			GraphicInterface->RequestReleaseRT(GRTSharedTextureRG16F);
		}

		const int32_t ShadowQuality = ConfigInterface->RenderingSetting().RTDirectionalShadowQuality;
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;

		GRTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, UH_FORMAT_R8_UNORM, true);
		GRTSharedTextureRG16F = GraphicInterface->RequestRenderTexture("RTSharedTextureRG16F", RenderResolution, UH_FORMAT_RG16F, true);

		if (!bInitOnly)
		{
			// need to rewrite descriptors after resize
			UpdateDescriptors();
		}
	}
}

void UHDeferredShadingRenderer::BuildTopLevelAS(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]);
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating
	TopLevelAS[CurrentFrameRT]->UpdateTopAS(RenderBuilder.GetCmdList(), CurrentFrameRT);
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]);
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	// transition output buffer to VK_IMAGE_LAYOUT_GENERAL
	RenderBuilder.ResourceBarrier(GRTSharedTextureRG16F, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	
	// TLAS bind
	RTShadowShader->BindTLAS(TopLevelAS[CurrentFrameRT].get(), 1, CurrentFrameRT);

	// bind descriptors and RT states
	std::vector<VkDescriptorSet> DescriptorSets = { RTShadowShader->GetDescriptorSet(CurrentFrameRT)
		, TextureTable->GetDescriptorSet(CurrentFrameRT)
		, SamplerTable->GetDescriptorSet(CurrentFrameRT)
		, RTVertexTable->GetDescriptorSet(CurrentFrameRT)
		, RTIndicesTable->GetDescriptorSet(CurrentFrameRT)
		, RTIndicesTypeTable->GetDescriptorSet(CurrentFrameRT)
		, RTMaterialDataTable->GetDescriptorSet(CurrentFrameRT) };

	RenderBuilder.BindRTDescriptorSet(RTShadowShader->GetPipelineLayout(), DescriptorSets);
	RenderBuilder.BindRTState(RTShadowShader->GetRTState());

	// trace!
	RenderBuilder.TraceRay(RTShadowExtent, RTShadowShader->GetRayGenTable(), RTShadowShader->GetMissTable(), RTShadowShader->GetHitGroupTable());

	// transition soften pass RTs
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSharedTextureRG16F, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();

	// dispatch softing pass
	UHComputeState* State = SoftRTShadowShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(SoftRTShadowShader->GetPipelineLayout(), SoftRTShadowShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RTShadowExtent.width, GThreadGroup2D_X)
		, MathHelpers::RoundUpDivide(RTShadowExtent.height, GThreadGroup2D_Y), 1);

	// finally, transition to shader read only
	RenderBuilder.ResourceBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}