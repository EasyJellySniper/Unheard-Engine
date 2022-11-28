#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::BuildTopLevelAS(UHGraphicBuilder& GraphBuilder)
{
	if (!GEnableRayTracing || RTInstanceCount == 0)
	{
		return;
	}

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	std::vector<UHMeshRendererComponent*> Renderers = CurrentScene->GetAllRenderers();
	TopLevelAS[CurrentFrame]->UpdateTopAS(GraphBuilder.GetCmdList(), CurrentFrame);

	// after update, shader descriptor for TLAS needs to be bound again
	RTShadowShader.BindTLAS(TopLevelAS[CurrentFrame].get(), 1, CurrentFrame);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::UpdateTopLevelAS]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif
}

void UHDeferredShadingRenderer::DispatchRayPass(UHGraphicBuilder& GraphBuilder)
{
	if (!GEnableRayTracing || !TopLevelAS[CurrentFrame] || RTInstanceCount == 0)
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
		, RTTextureTable.GetDescriptorSet(CurrentFrame)
		, RTSamplerTable.GetDescriptorSet(CurrentFrame)
		, RTVertexTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTable.GetDescriptorSet(CurrentFrame)
		, RTIndicesTypeTable.GetDescriptorSet(CurrentFrame) };

	GraphBuilder.BindRTDescriptorSet(RTShadowShader.GetPipelineLayout(), DescriptorSets);
	GraphBuilder.BindRTState(RTShadowShader.GetRTState());

	// trace!
	GraphBuilder.TraceRay(RTShadowExtent, RTShadowShader.GetRayGenTable(), RTShadowShader.GetHitGroupTable());

	// transition to shader read after tracing
	GraphBuilder.ResourceBarrier(RTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::RayTracingShadow]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif
}