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

	TopLevelAS[CurrentFrame]->UpdateTopAS(GraphBuilder.GetCmdList(), CurrentFrame);

	// after update, shader descriptor for TLAS needs to be bound again
	RTShadowShader.BindTLAS(TopLevelAS[CurrentFrame].get(), 1, CurrentFrame);
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHGraphicBuilder& GraphBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || !TopLevelAS[CurrentFrame] || RTInstanceCount == 0)
	{
		return;
	}

	UHGPUTimeQueryScope TimeScope(GraphBuilder.GetCmdList(), GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]);

	// transition RW buffer to VK_IMAGE_LAYOUT_GENERAL
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	GraphBuilder.ResourceBarrier(RTTranslucentShadow, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// bind descriptors and RT states
	std::vector<VkDescriptorSet> DescriptorSets = { RTShadowShader.GetDescriptorSet(CurrentFrame)
		, TextureTable.GetDescriptorSet(CurrentFrame)
		, SamplerTable.GetDescriptorSet(CurrentFrame)
		, RTVertexTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTypeTable.GetDescriptorSet(CurrentFrame)
		, RTMaterialDataTable.GetDescriptorSet(CurrentFrame) };

	GraphBuilder.BindRTDescriptorSet(RTShadowShader.GetPipelineLayout(), DescriptorSets);
	GraphBuilder.BindRTState(RTShadowShader.GetRTState());

	// trace!
	GraphBuilder.TraceRay(RTShadowExtent, RTShadowShader.GetRayGenTable(), RTShadowShader.GetMissTable(), RTShadowShader.GetHitGroupTable());

	// transition to shader read after tracing
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(RTTranslucentShadow, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// soften shadow after trace is done, redundant transition is needed to sync the result in GPU
	// otherwise the writing from compute shader won't work
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
	GraphBuilder.ResourceBarrier(RTTranslucentShadow, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	// note that this is dispatched with render resolution
	UHComputeState* State = SoftRTShadowShader.GetComputeState();
	GraphBuilder.BindComputeState(State);
	GraphBuilder.BindDescriptorSetCompute(SoftRTShadowShader.GetPipelineLayout(), SoftRTShadowShader.GetDescriptorSet(CurrentFrame));
	GraphBuilder.Dispatch((RenderResolution.width + GThreadGroup2D_X) / GThreadGroup2D_X, (RenderResolution.height + GThreadGroup2D_Y) / GThreadGroup2D_Y, 1);

	// finally, transition to shader read only
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(RTTranslucentShadow, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}