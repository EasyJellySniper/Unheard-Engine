#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::BuildTopLevelAS(UHGraphicBuilder& GraphBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating

	UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]);

	TopLevelAS[CurrentFrameRT]->UpdateTopAS(GraphBuilder.GetCmdList(), CurrentFrameRT);
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHGraphicBuilder& GraphBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || RTInstanceCount == 0)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]);

	// transition output buffer to VK_IMAGE_LAYOUT_GENERAL
	GraphBuilder.ResourceBarrier(RTSharedTextureRG16F, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	
	// after update, shader descriptor for TLAS needs to be bound again
	// note that the AS is built in async compute queue, need to bind the previous result here
	if (bEnableAsyncComputeRT)
	{
		RTShadowShader.BindTLAS(TopLevelAS[1 - CurrentFrameRT].get(), 1, CurrentFrameRT);
	}

	// bind descriptors and RT states
	std::vector<VkDescriptorSet> DescriptorSets = { RTShadowShader.GetDescriptorSet(CurrentFrameRT)
		, TextureTable.GetDescriptorSet(CurrentFrameRT)
		, SamplerTable.GetDescriptorSet(CurrentFrameRT)
		, RTVertexTable.GetDescriptorSet(CurrentFrameRT)
		, RTIndicesTable.GetDescriptorSet(CurrentFrameRT)
		, RTIndicesTypeTable.GetDescriptorSet(CurrentFrameRT)
		, RTMaterialDataTable.GetDescriptorSet(CurrentFrameRT) };

	GraphBuilder.BindRTDescriptorSet(RTShadowShader.GetPipelineLayout(), DescriptorSets);
	GraphBuilder.BindRTState(RTShadowShader.GetRTState());

	// trace!
	GraphBuilder.TraceRay(RTShadowExtent, RTShadowShader.GetRayGenTable(), RTShadowShader.GetMissTable(), RTShadowShader.GetHitGroupTable());

	// transition soften pass RTs
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	GraphBuilder.ResourceBarrier(RTSharedTextureRG16F, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// dispatch softing pass
	UHComputeState* State = SoftRTShadowShader.GetComputeState();
	GraphBuilder.BindComputeState(State);
	GraphBuilder.BindDescriptorSetCompute(SoftRTShadowShader.GetPipelineLayout(), SoftRTShadowShader.GetDescriptorSet(CurrentFrameRT));
	GraphBuilder.Dispatch((RTShadowExtent.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RTShadowExtent.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

	// finally, transition to shader read only
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}