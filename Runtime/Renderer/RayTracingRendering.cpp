#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::ReleaseRayTracingBuffers()
{
	if (!GraphicInterface->IsRayTracingEnabled())
	{
		return;
	}

	UH_SAFE_RELEASE_TEX(GRTShadowResult);
	UH_SAFE_RELEASE_TEX(GRTSharedTextureRG);
	UH_SAFE_RELEASE_TEX(GRTReflectionResult);
	UH_SAFE_RELEASE_TEX(GSmoothSceneNormal);
	UH_SAFE_RELEASE_TEX(GRTIndirectLighting);

	// cleanup gaussian constant
	RTGaussianConsts.Release(GraphicInterface);
}

void UHDeferredShadingRenderer::InitRTFilterConstants()
{
	UHTextureFormat TempRTFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;

	// initialize gaussian consts, mainly use for reflection
	RTGaussianConsts.GBlurRadius = 1;
	RTGaussianConsts.IterationCount = 1;
	CalculateBlurWeights(RTGaussianConsts.GBlurRadius, RTGaussianConsts.Weights);

	RTGaussianConsts.GaussianFilterTempRT0.resize(GRTReflectionResult->GetMipMapCount());
	RTGaussianConsts.GaussianFilterTempRT1.resize(GRTReflectionResult->GetMipMapCount());

	VkExtent2D FilterResolution;

	// for RT use
	for (uint32_t Idx = 2; Idx < GRTReflectionResult->GetMipMapCount(); Idx++)
	{
		FilterResolution.width = RenderResolution.width >> Idx;
		FilterResolution.height = RenderResolution.height >> Idx;

		RTGaussianConsts.GaussianFilterTempRT0[Idx] =
			GraphicInterface->RequestRenderTexture("GaussianFilterTempRT0", FilterResolution, TempRTFormat, true);

		RTGaussianConsts.GaussianFilterTempRT1[Idx] =
			GraphicInterface->RequestRenderTexture("GaussianFilterTempRT1", FilterResolution, TempRTFormat, true);
	}
}

void UHDeferredShadingRenderer::ResizeRayTracingBuffers(bool bUpdateDescriptor)
{
	if (GraphicInterface->IsRayTracingEnabled())
	{
		GraphicInterface->WaitGPU();
		ReleaseRayTracingBuffers();

		const int32_t ShadowQuality = ConfigInterface->RenderingSetting().RTShadowQuality;
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;

		GRTShadowResult = GraphicInterface->RequestRenderTexture("RTShadowResult", RTShadowExtent, UHTextureFormat::UH_FORMAT_R8_UNORM, true);
		GRTSharedTextureRG = GraphicInterface->RequestRenderTexture("RTSharedTextureRG", RenderResolution, UHTextureFormat::UH_FORMAT_RG16F, true);
		GRTReflectionResult = GraphicInterface->RequestRenderTexture("RTReflectionResult", RenderResolution, UHTextureFormat::UH_FORMAT_RGBA8_UNORM, true, true);

		// refined normal at half size
		VkExtent2D HalfRes = RenderResolution;
		HalfRes.width >>= 1;
		HalfRes.height >>= 1;
		GSmoothSceneNormal = GraphicInterface->RequestRenderTexture("SmoothSceneNormal", HalfRes, UHTextureFormat::UH_FORMAT_RGBA16F, true);

		// IL rendering is done at quater res now
		RTIndirectLightingExtent.width = RenderResolution.width >> 2;
		RTIndirectLightingExtent.height = RenderResolution.height >> 2;
		GRTIndirectLighting = GraphicInterface->RequestRenderTexture("RTIndirectLighting", RTIndirectLightingExtent, UHTextureFormat::UH_FORMAT_RGBA8_UNORM
			, true, false, MaxNumOfIndirectLightRays);

		InitRTFilterConstants();

		if (bUpdateDescriptor)
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
	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Build TopLevel AS");

	// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#general-tips-for-building-acceleration-structures
	// From Microsoft tips: Rebuild top-level acceleration structure every frame
	// but I still choose to update AS instead of rebuilding, FPS is higher with updating
	GTopLevelAS[CurrentFrameRT]->UpdateTopAS(RenderBuilder.GetCmdList(), CurrentFrameRT, RTParams.RTCullingDistance);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::CollectLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("CollectLightPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::CollectLightPass)], "CollectLightPass");
	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0)
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
	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0 || !RTParams.bEnableRTShadow)
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

	// if it's half res tracing, upsample it before proceed to softing pass
	if (RTShadowExtent.width != RenderResolution.width)
	{
		RenderBuilder.BindComputeState(UpsampleNearest2x2Shader->GetComputeState());

		UHUpsampleConstants Consts;
		Consts.OutputResolutionX = RenderResolution.width;
		Consts.OutputResolutionY = RenderResolution.height;
		UpsampleNearest2x2Shader->BindParameters(RenderBuilder, CurrentFrameRT, GRTSharedTextureRG, Consts);

		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y)
			, 1);
	}

	// transition soften pass RTs
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSharedTextureRG, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();

	// dispatch softing pass
	UHComputeState* State = SoftRTShadowShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(SoftRTShadowShader->GetPipelineLayout(), SoftRTShadowShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RTShadowExtent.width, GThreadGroup2D_X)
		, MathHelpers::RoundUpDivide(RTShadowExtent.height, GThreadGroup2D_Y)
		, 1);

	// finally, transition to shader read only
	RenderBuilder.ResourceBarrier(GRTShadowResult, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchSmoothSceneNormalPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("SmoothSceneNormalPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::SmoothSceneNormalPass)], "SmoothSceneNormalPass");

	if (!RTParams.bEnableRayTracing || !RTParams.bEnableRTDenoise)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Smooth scene normal pass");
	RenderBuilder.ResourceBarrier(GSmoothSceneNormal, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	// two pass refine normal at half size
	VkExtent2D HalfRes = RenderResolution;
	HalfRes.width >>= 1;
	HalfRes.height >>= 1;

	UHComputeState* State = RTSmoothSceneNormalHShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(RTSmoothSceneNormalHShader->GetPipelineLayout(), RTSmoothSceneNormalHShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(HalfRes.width, GThreadGroup1D), HalfRes.height, 1);

	State = RTSmoothSceneNormalVShader->GetComputeState();
	RenderBuilder.BindComputeState(State);
	RenderBuilder.BindDescriptorSetCompute(RTSmoothSceneNormalVShader->GetPipelineLayout(), RTSmoothSceneNormalVShader->GetDescriptorSet(CurrentFrameRT));
	RenderBuilder.Dispatch(HalfRes.width, MathHelpers::RoundUpDivide(HalfRes.height, GThreadGroup1D), 1);

	RenderBuilder.ResourceBarrier(GSmoothSceneNormal, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayIndirectLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RayIndirectLightPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingIndirectLight)], "RayTracingIndirectLight");

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Indirect Light");

	// transition to UAV ready
	RenderBuilder.ResourceBarrier(GRTIndirectLighting, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	RenderBuilder.ClearRenderTexture(GRTIndirectLighting, GBlackClearColor, RTParams.FrameNumber & 3);
	RenderBuilder.ResourceBarrier(GRTIndirectLighting, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	if (RTParams.bEnableRayTracing && RTInstanceCount > 0 && RTParams.bEnableRTIndirectLighting)
	{
		// [0] is to bind the shader descriptor
		RTDescriptorSets[CurrentFrameRT][0] = RTIndirectLightShader->GetDescriptorSet(CurrentFrameRT);
		RenderBuilder.BindRTDescriptorSet(RTIndirectLightShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
		RenderBuilder.BindRTState(RTIndirectLightShader->GetRTState());

		// trace the ray!
		RenderBuilder.TraceRay(RTIndirectLightingExtent, RTIndirectLightShader->GetRayGenTable(), RTIndirectLightShader->GetMissTable(), RTIndirectLightShader->GetHitGroupTable());
	}

	RenderBuilder.ResourceBarrier(GRTIndirectLighting, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayReflectionPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("DispatchRayReflectionPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingReflection)], "RayTracingReflection");

	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0 || !RTParams.bEnableRTReflection)
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
	RenderBuilder.ClearRenderTexture(GRTReflectionResult, GTransparentClearColor);
	RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	// bind descriptors and RT states
	// [0] is to bind the shader descriptor, and prevent touching other elements. Not a typo!
	RTDescriptorSets[CurrentFrameRT][0] = RTReflectionShader->GetDescriptorSet(CurrentFrameRT);
	RenderBuilder.BindRTDescriptorSet(RTReflectionShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
	RenderBuilder.BindRTState(RTReflectionShader->GetRTState());

	// trace! it will do full, full temopral or just half base on quality.
	// and does upsample if necessary
	UHUpsampleConstants Consts;
	Consts.OutputResolutionX = RenderResolution.width;
	Consts.OutputResolutionY = RenderResolution.height;

	if (RTParams.RTReflectionQuality == UH_ENUM_VALUE(UHRTReflectionQuality::RTReflection_Half))
	{
		VkExtent2D HalfRes = RenderResolution;
		HalfRes.width >>= 1;
		HalfRes.height >>= 1;
		RenderBuilder.TraceRay(HalfRes, RTReflectionShader->GetRayGenTable(), RTReflectionShader->GetMissTable(), RTReflectionShader->GetHitGroupTable());

		// Upsample after the half res tracing
		RenderBuilder.BindComputeState(UpsampleNearest2x2Shader->GetComputeState());
		UpsampleNearest2x2Shader->BindParameters(RenderBuilder, CurrentFrameRT, GRTReflectionResult, Consts);

		RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y)
			, 1);
	}
	else
	{
		RenderBuilder.TraceRay(RenderResolution, RTReflectionShader->GetRayGenTable(), RTReflectionShader->GetMissTable(), RTReflectionShader->GetHitGroupTable());

		if (RTParams.RTReflectionQuality == UH_ENUM_VALUE(UHRTReflectionQuality::RTReflection_FullTemporal))
		{
			// Upsample after the half pixel tracing
			RenderBuilder.BindComputeState(UpsampleNearestHShader->GetComputeState());
			UpsampleNearestHShader->BindParameters(RenderBuilder, CurrentFrameRT, GRTReflectionResult, Consts);

			RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
				, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y)
				, 1);
		}
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
			RenderBuilder.PushConstant(RTReflectionMipmapShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UHMipmapConstants), &Constant);
			RTReflectionMipmapShader->BindTextures(RenderBuilder, GRTReflectionResult, GRTReflectionResult, Mdx, CurrentFrameRT);

			RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, Mdx);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(MipExtent.width, GThreadGroup2D_X), MathHelpers::RoundUpDivide(MipExtent.height, GThreadGroup2D_Y), 1);
		}

		for (uint32_t Mdx = 2; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RTGaussianConsts.GBlurResolution[0] = RenderResolution.width >> Mdx;
			RTGaussianConsts.GBlurResolution[1] = RenderResolution.height >> Mdx;
			RTGaussianConsts.InputMip = Mdx;
			DispatchGaussianFilter(RenderBuilder, "Blur Reflection Mip " + std::to_string(Mdx), GRTReflectionResult, GRTReflectionResult, RTGaussianConsts);
		}

		for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
		}
		RenderBuilder.FlushResourceBarrier();
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}