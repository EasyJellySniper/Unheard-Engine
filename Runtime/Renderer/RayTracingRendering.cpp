#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::BuildTopLevelAS(UHRenderBuilder& RenderBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]);

	TopLevelAS[CurrentFrameRT]->UpdateTopAS(RenderBuilder.GetCmdList(), CurrentFrameRT);
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHRenderBuilder& RenderBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]);

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
	RenderBuilder.Dispatch((RTShadowExtent.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RTShadowExtent.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

	// finally, transition to shader read only
	RenderBuilder.ResourceBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}