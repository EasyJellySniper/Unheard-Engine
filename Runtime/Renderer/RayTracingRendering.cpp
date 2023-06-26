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

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	TopLevelAS[CurrentFrame]->UpdateTopAS(GraphBuilder.GetCmdList(), CurrentFrame);

	// after update, shader descriptor for TLAS needs to be bound again
	if (GraphicInterface->IsRayTracingOcclusionTestEnabled())
	{
		RTOcclusionTestShader.BindTLAS(TopLevelAS[CurrentFrame].get(), 1, CurrentFrame);
	}

	RTShadowShader.BindTLAS(TopLevelAS[CurrentFrame].get(), 1, CurrentFrame);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif
}

void UHDeferredShadingRenderer::DispatchRayOcclusionTestPass(UHGraphicBuilder& GraphBuilder)
{
	if (!GraphicInterface->IsRayTracingOcclusionTestEnabled() || !TopLevelAS[CurrentFrame] || RTInstanceCount == 0)
	{
		return;
	}

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::RayTracingOcclusionTest]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	// reset occlusion buffer
	GraphBuilder.ClearUAVBuffer(OcclusionVisibleBuffer->GetBuffer(), 0);

	// bind descriptors and RT states
	std::vector<VkDescriptorSet> DescriptorSets = { RTOcclusionTestShader.GetDescriptorSet(CurrentFrame)
		, TextureTable.GetDescriptorSet(CurrentFrame)
		, SamplerTable.GetDescriptorSet(CurrentFrame)
		, RTVertexTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTypeTable.GetDescriptorSet(CurrentFrame)
		, RTMaterialDataTable.GetDescriptorSet(CurrentFrame) };

	GraphBuilder.BindRTDescriptorSet(RTOcclusionTestShader.GetPipelineLayout(), DescriptorSets);
	GraphBuilder.BindRTState(RTOcclusionTestShader.GetRTState());

	// trace!
	VkExtent2D RTOTExtent;
	RTOTExtent.width = RenderResolution.width / 2;
	RTOTExtent.height = RenderResolution.height / 2;
	GraphBuilder.TraceRay(RTOTExtent, RTOcclusionTestShader.GetRayGenTable(), RTOcclusionTestShader.GetMissTable(), RTOcclusionTestShader.GetHitGroupTable());

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::RayTracingOcclusionTest]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHGraphicBuilder& GraphBuilder)
{
	if (!GraphicInterface->IsRayTracingEnabled() || !TopLevelAS[CurrentFrame] || RTInstanceCount == 0)
	{
		return;
	}

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	// transition RW buffer to VK_IMAGE_LAYOUT_GENERAL
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

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

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif
}