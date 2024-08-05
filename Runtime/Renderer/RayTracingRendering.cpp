#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::ReleaseRayTracingBuffers()
{
	if (!GraphicInterface->IsRayTracingEnabled())
	{
		return;
	}

	GraphicInterface->RequestReleaseRT(GRTShadowResult);
	GraphicInterface->RequestReleaseRT(GRTSharedTextureRG);
	GraphicInterface->RequestReleaseRT(GRTReflectionResult);
}

void UHDeferredShadingRenderer::ResizeRayTracingBuffers(bool bInitOnly)
{
	if (GraphicInterface->IsRayTracingEnabled())
	{
		if (!bInitOnly)
		{
			GraphicInterface->WaitGPU();
			ReleaseRayTracingBuffers();
		}

		const int32_t ShadowQuality = ConfigInterface->RenderingSetting().RTShadowQuality;
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;

		GRTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, UHTextureFormat::UH_FORMAT_R8_UNORM, true);
		GRTSharedTextureRG = GraphicInterface->RequestRenderTexture("RTSharedTextureRG", RenderResolution, UHTextureFormat::UH_FORMAT_RG16F, true);
		GRTReflectionResult = GraphicInterface->RequestRenderTexture("RTSharedTextureFloat", RenderResolution, UHTextureFormat::UH_FORMAT_RGBA16F, true, true);

		if (!bInitOnly)
		{
			// need to rewrite descriptors after resize
			UpdateDescriptors();
		}
	}
}

void UHDeferredShadingRenderer::BuildTopLevelAS(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::UpdateTopLevelAS)]);
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		return;
	}

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating
	GTopLevelAS[CurrentFrameRT]->UpdateTopAS(RenderBuilder.GetCmdList(), CurrentFrameRT, RTCullingDistanceRT);
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingShadow)]);
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		if (GRTShadowResult != nullptr)
		{
			RenderBuilder.ResourceBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		return;
	}

	// transition output buffer to VK_IMAGE_LAYOUT_GENERAL
	RenderBuilder.ResourceBarrier(GRTSharedTextureRG, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// bind descriptors and RT states
	// [0] is to bind the shader descriptor, and prevent touching other elements. Not a typo!
	RTDescriptorSets[CurrentFrameRT][0] = RTShadowShader->GetDescriptorSet(CurrentFrameRT);
	RenderBuilder.BindRTDescriptorSet(RTShadowShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
	RenderBuilder.BindRTState(RTShadowShader->GetRTState());

	// trace!
	RenderBuilder.TraceRay(RTShadowExtent, RTShadowShader->GetRayGenTable(), RTShadowShader->GetMissTable(), RTShadowShader->GetHitGroupTable());

	// transition soften pass RTs
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSharedTextureRG, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
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

void UHDeferredShadingRenderer::DispatchRayReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingReflection)]);
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		if (GRTReflectionResult != nullptr)
		{
			for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
			{
				RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
			}
			RenderBuilder.FlushResourceBarrier();
		}
		return;
	}

	// buffer transition and TLAS bind
	for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
	{
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_GENERAL, Mdx));
	}
	RenderBuilder.FlushResourceBarrier();

	// bind descriptors and RT states
	// [0] is to bind the shader descriptor, and prevent touching other elements. Not a typo!
	RTDescriptorSets[CurrentFrameRT][0] = RTReflectionShader->GetDescriptorSet(CurrentFrameRT);
	RenderBuilder.BindRTDescriptorSet(RTReflectionShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
	RenderBuilder.BindRTState(RTReflectionShader->GetRTState());

	// trace! it will do full, full temopral or just half base on quality
	if (RTReflectionQualityRT == UH_ENUM_VALUE(UHRTReflectionQuality::RTReflection_Half))
	{
		VkExtent2D HalfRes = RenderResolution;
		HalfRes.width >>= 1;
		HalfRes.height >>= 1;
		RenderBuilder.TraceRay(HalfRes, RTReflectionShader->GetRayGenTable(), RTReflectionShader->GetMissTable(), RTReflectionShader->GetHitGroupTable());
	}
	else
	{
		RenderBuilder.TraceRay(RenderResolution, RTReflectionShader->GetRayGenTable(), RTReflectionShader->GetMissTable(), RTReflectionShader->GetHitGroupTable());
	}

	// generate mips with custom shader and blur the mips [2, Max]
	{
		// force a resource sync for GRTReflectionResult
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.FlushResourceBarrier();
		RenderBuilder.BindComputeState(RTReflectionMipmapShader->GetComputeState());
		
		for (uint32_t Mdx = 1; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			VkExtent2D MipExtent = GRTReflectionResult->GetExtent();
			MipExtent.width >>= Mdx;
			MipExtent.height >>= Mdx;

			UHMipmapConstants Constant;
			Constant.MipWidth = MipExtent.width;
			Constant.MipHeight = MipExtent.height;
			Constant.bUseAlphaWeight = (Mdx < 3) ? 1 : 0;

			// push constant and descriptor in fly
			vkCmdPushConstants(RenderBuilder.GetCmdList(), RTReflectionMipmapShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(UHMipmapConstants), &Constant);
			RTReflectionMipmapShader->BindTextures(RenderBuilder, GRTReflectionResult, GRTReflectionResult, Mdx, CurrentFrameRT);

			RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, Mdx);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(MipExtent.width, GThreadGroup2D_X), MathHelpers::RoundUpDivide(MipExtent.height, GThreadGroup2D_Y), 1);
		}

		UHGaussianFilterConstants Constant;
		Constant.GBlurRadius = 2;
		Constant.IterationCount = 2;
		Constant.TempRTFormat = bHDREnabledRT ? UHTextureFormat::UH_FORMAT_RGBA16F : UHTextureFormat::UH_FORMAT_RGBA8_UNORM;
		CalculateBlurWeights(Constant.GBlurRadius, Constant.Weights);

		for (uint32_t Mdx = 2; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			Constant.GBlurResolution[0] = RenderResolution.width >> Mdx;
			Constant.GBlurResolution[1] = RenderResolution.height >> Mdx;
			Constant.InputMip = Mdx;
			DispatchGaussianFilter(RenderBuilder, "Blur Reflection Mip " + std::to_string(Mdx), GRTReflectionResult, GRTReflectionResult, Constant);
		}

		for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
		}
		RenderBuilder.FlushResourceBarrier();
	}
}