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
	GraphicInterface->RequestReleaseRT(GSmoothReflectVector);
}

void UHDeferredShadingRenderer::InitRTGaussianConstants()
{
	UHTextureFormat TempRTFormat = bHDREnabledRT ? UHTextureFormat::UH_FORMAT_RGBA16F : UHTextureFormat::UH_FORMAT_RGBA8_UNORM;

	// initialize gaussian consts
	RayTracingGaussianConsts.GBlurRadius = 2;
	RayTracingGaussianConsts.IterationCount = 2;
	CalculateBlurWeights(RayTracingGaussianConsts.GBlurRadius, RayTracingGaussianConsts.Weights);

	RayTracingGaussianConsts.GaussianFilterTempRT0.resize(GRTReflectionResult->GetMipMapCount());
	RayTracingGaussianConsts.GaussianFilterTempRT1.resize(GRTReflectionResult->GetMipMapCount());

	VkExtent2D FilterResolution;

	// for RT use
	for (uint32_t Idx = 2; Idx < GRTReflectionResult->GetMipMapCount(); Idx++)
	{
		FilterResolution.width = RenderResolution.width >> Idx;
		FilterResolution.height = RenderResolution.height >> Idx;

		RayTracingGaussianConsts.GaussianFilterTempRT0[Idx] =
			GraphicInterface->RequestRenderTexture("GaussianFilterTempRT0", FilterResolution, TempRTFormat, true);

		RayTracingGaussianConsts.GaussianFilterTempRT1[Idx] =
			GraphicInterface->RequestRenderTexture("GaussianFilterTempRT1", FilterResolution, TempRTFormat, true);
	}
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

		// refined normal at half size
		VkExtent2D HalfRes = RenderResolution;
		HalfRes.width >>= 1;
		HalfRes.height >>= 1;
		GSmoothReflectVector = GraphicInterface->RequestRenderTexture("SmoothReflectVector", HalfRes, UHTextureFormat::UH_FORMAT_RGBA16F, true);

		InitRTGaussianConstants();
		if (!bInitOnly)
		{
			// need to rewrite descriptors after resize
			UpdateDescriptors();
		}
	}
}

void UHDeferredShadingRenderer::BuildTopLevelAS(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("BuildTopLevelAS", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::UpdateTopLevelAS)], "UpdateTopLevelAS");
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Build TopLevel AS");

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating
	GTopLevelAS[CurrentFrameRT]->UpdateTopAS(RenderBuilder.GetCmdList(), CurrentFrameRT, RTCullingDistanceRT);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::CollectLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("CollectLightPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::CollectLightPass)], "CollectLightPass");
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Collect Light");

	if (CurrentScene->GetPointLightCount() > 0 || CurrentScene->GetSpotLightCount() > 0)
	{
		RenderBuilder.ClearUAVBuffer(GInstanceLightsBuffer[CurrentFrameRT]->GetBuffer(), ~0);
	}

	// point light collection
	if (CurrentScene->GetPointLightCount() > 0)
	{
		// bind state
		UHComputeState* State = CollectPointLightShader->GetComputeState();
		RenderBuilder.BindComputeState(State);

		// bind sets
		RenderBuilder.BindDescriptorSetCompute(CollectPointLightShader->GetPipelineLayout(), CollectPointLightShader->GetDescriptorSet(CurrentFrameRT));

		// refactor this if there is a chance that RTInstanceCount > 64K
		const uint32_t Gy = MathHelpers::RoundUpDivide(static_cast<uint32_t>(CurrentScene->GetPointLightCount()), GThreadGroup1D);
		RenderBuilder.Dispatch(RTInstanceCount, Gy, 1);
	}

	// spot light collection
	if (CurrentScene->GetSpotLightCount() > 0)
	{
		// bind state
		UHComputeState* State = CollectSpotLightShader->GetComputeState();
		RenderBuilder.BindComputeState(State);

		// bind sets
		RenderBuilder.BindDescriptorSetCompute(CollectSpotLightShader->GetPipelineLayout(), CollectSpotLightShader->GetDescriptorSet(CurrentFrameRT));

		// refactor this if there is a chance that RTInstanceCount > 64K
		const uint32_t Gy = MathHelpers::RoundUpDivide(static_cast<uint32_t>(CurrentScene->GetSpotLightCount()), GThreadGroup1D);
		RenderBuilder.Dispatch(RTInstanceCount, Gy, 1);
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayShadowPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("DispatchRayShadowPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingShadow)], "RayTracingShadow");
	if (!bIsRaytracingEnableRT || RTInstanceCount == 0)
	{
		if (GRTShadowResult != nullptr)
		{
			RenderBuilder.ResourceBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Shadow");

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

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchSmoothReflectVectorPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("SmoothReflectVectorPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::SmoothReflectVectorPass)], "SmoothReflectVectorPass");

	if (!bIsRaytracingEnableRT)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Smooth reflect vector pass");
	RenderBuilder.ResourceBarrier(GSmoothReflectVector, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// two pass refine normal at half size
	VkExtent2D HalfRes = RenderResolution;
	HalfRes.width >>= 1;
	HalfRes.height >>= 1;

	UHComputeState* State = RTSmoothReflectHShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(RTSmoothReflectHShader->GetPipelineLayout(), RTSmoothReflectHShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(HalfRes.width, GThreadGroup1D), HalfRes.height, 1);

	State = RTSmoothReflectVShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(RTSmoothReflectVShader->GetPipelineLayout(), RTSmoothReflectVShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(HalfRes.width, MathHelpers::RoundUpDivide(HalfRes.height, GThreadGroup1D), 1);

	RenderBuilder.ResourceBarrier(GSmoothReflectVector, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("DispatchRayReflectionPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingReflection)], "RayTracingReflection");

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

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Reflection And Blur");

	// clear and transition output buffer to VK_IMAGE_LAYOUT_GENERAL
	RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	RenderBuilder.ClearRenderTexture(GRTReflectionResult);
	RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

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

		for (uint32_t Mdx = 2; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RayTracingGaussianConsts.GBlurResolution[0] = RenderResolution.width >> Mdx;
			RayTracingGaussianConsts.GBlurResolution[1] = RenderResolution.height >> Mdx;
			RayTracingGaussianConsts.InputMip = Mdx;
			DispatchGaussianFilter(RenderBuilder, "Blur Reflection Mip " + std::to_string(Mdx), GRTReflectionResult, GRTReflectionResult, RayTracingGaussianConsts);
		}

		for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
		}
		RenderBuilder.FlushResourceBarrier();
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}