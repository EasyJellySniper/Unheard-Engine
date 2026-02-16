#pragma once
#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::ReleaseRayTracingBuffers()
{
	if (!GraphicInterface->IsRayTracingEnabled())
	{
		return;
	}

	UH_SAFE_RELEASE_TEX(GRTSoftShadow);
	UH_SAFE_RELEASE_TEX(GRTShadowData);
	UH_SAFE_RELEASE_TEX(GRTReceiveLightBits);
	UH_SAFE_RELEASE_TEX(GRTReflectionResult);
	UH_SAFE_RELEASE_TEX(GSmoothSceneNormal);

	UH_SAFE_RELEASE_TEX(GRTIndirectDiffuse);
	UH_SAFE_RELEASE_TEX(GRTIndirectDiffuseHistory);
	UH_SAFE_RELEASE_TEX(GRTIndirectOcclusion);
	UH_SAFE_RELEASE_TEX(GRTIndirectOcclusionHistory);

	UH_SAFE_RELEASE_TEX(GRTSkyData);
	UH_SAFE_RELEASE_TEX(GRTSkyDiscoverAngle);

	RTIndirectDiffuseBFConsts.Release(GraphicInterface);
	RTIndirectOcclusionBFConsts.Release(GraphicInterface);
}

void UHDeferredShadingRenderer::ResizeRayTracingBuffers(bool bUpdateDescriptor)
{
	if (GraphicInterface->IsRayTracingEnabled())
	{
		GraphicInterface->WaitGPU();
		ReleaseRayTracingBuffers();

		const int32_t ShadowQuality = ConfigInterface->RenderingSetting().RTShadowQuality;
		RTShadowExtent = RenderResolution;
		RTShadowExtent.width = RenderResolution.width >> ShadowQuality;
		RTShadowExtent.height = RenderResolution.height >> ShadowQuality;

		VkExtent2D HalfRes = RenderResolution;
		HalfRes.width >>= 1;
		HalfRes.height >>= 1;

		UHRenderTextureSettings RenderTextureSetting{};
		RenderTextureSetting.bIsReadWrite = true;

		// RT shadow data and soft shadow, currently a number of softed RT shadows per pixel are supported
		RenderTextureSetting.NumSlices = UHLightPassShader::MaxSoftShadowLightsPerPixel;
		GRTShadowData = GraphicInterface->RequestRenderTexture("RTShadowData", RTShadowExtent, UHTextureFormat::UH_FORMAT_RG16F, RenderTextureSetting);
		GRTSoftShadow = GraphicInterface->RequestRenderTexture("RTSoftShadow", RTShadowExtent, UHTextureFormat::UH_FORMAT_R8_UNORM, RenderTextureSetting);

		RenderTextureSetting.NumSlices = 1;
		GRTReceiveLightBits = GraphicInterface->RequestRenderTexture("RTReceiveLightBits", RTShadowExtent, UHTextureFormat::UH_FORMAT_R32_UINT, RenderTextureSetting);

		// reflection buffer setup, the size is the same as rendering resolution regardless the quality setting
		// as there are some filter passes for reflections
		RenderTextureSetting.bUseMipmap = true;
		GRTReflectionResult = GraphicInterface->RequestRenderTexture("RTReflectionResult", RenderResolution, UHTextureFormat::UH_FORMAT_RGBA8_UNORM, RenderTextureSetting);
		RenderTextureSetting.bUseMipmap = false;

		// refined normal at half size
		GSmoothSceneNormal = GraphicInterface->RequestRenderTexture("SmoothSceneNormal", HalfRes, UHTextureFormat::UH_FORMAT_RGBA16F, RenderTextureSetting);

		// indirect light at half size
		RTIndirectLightExtent = HalfRes;

		GRTIndirectDiffuse = GraphicInterface->RequestRenderTexture("RTIndirectDiffuse"
			, RTIndirectLightExtent
			, UHTextureFormat::UH_FORMAT_RGBA16F
			, RenderTextureSetting);

		GRTIndirectDiffuseHistory = GraphicInterface->RequestRenderTexture("RTIndirectDiffuseHistory"
			, RTIndirectLightExtent
			, GRTIndirectDiffuse->GetFormat()
			, RenderTextureSetting);

		GRTIndirectOcclusion = GraphicInterface->RequestRenderTexture("RTIndirectOcclusion"
			, RTIndirectLightExtent
			, UHTextureFormat::UH_FORMAT_RG16F
			, RenderTextureSetting);

		GRTIndirectOcclusionHistory = GraphicInterface->RequestRenderTexture("RTIndirectOcclusionHistory"
			, RTIndirectLightExtent
			, GRTIndirectOcclusion->GetFormat()
			, RenderTextureSetting);

		// initialize UHBilateralFilterConstants for filtering RTDiffuse result
		RTIndirectDiffuseBFConsts.FilterResolution[0] = RTIndirectLightExtent.width;
		RTIndirectDiffuseBFConsts.FilterResolution[1] = RTIndirectLightExtent.height;
		RTIndirectDiffuseBFConsts.KernalRadius = 4;
		RTIndirectDiffuseBFConsts.SceneBufferUpsample = RenderResolution.width / RTIndirectLightExtent.width;
		RTIndirectDiffuseBFConsts.SigmaS = 3.5f;
		RTIndirectDiffuseBFConsts.SigmaD = 0.12f;
		RTIndirectDiffuseBFConsts.SigmaN = 8.0f;
		RTIndirectDiffuseBFConsts.SigmaC = 2.0f;
		RTIndirectDiffuseBFConsts.IterationCount = 2;

		RTIndirectDiffuseBFConsts.FilterTempRT0 = GraphicInterface->RequestRenderTexture("RTDiffuse_FilterTempRT0"
			, RTIndirectLightExtent
			, GRTIndirectDiffuse->GetFormat()
			, RenderTextureSetting);

		RTIndirectDiffuseBFConsts.FilterTempRT1 = GraphicInterface->RequestRenderTexture("RTDiffuse_FilterTempRT1"
			, RTIndirectLightExtent
			, GRTIndirectDiffuse->GetFormat()
			, RenderTextureSetting);

		// initialize UHBilateralFilterConstants for filtering RTAO result
		RTIndirectOcclusionBFConsts.FilterResolution[0] = RTIndirectLightExtent.width;
		RTIndirectOcclusionBFConsts.FilterResolution[1] = RTIndirectLightExtent.height;
		RTIndirectOcclusionBFConsts.KernalRadius = 2;
		RTIndirectOcclusionBFConsts.SceneBufferUpsample = RenderResolution.width / RTIndirectLightExtent.width;
		RTIndirectOcclusionBFConsts.SigmaS = 3.0f;
		RTIndirectOcclusionBFConsts.SigmaD = 0.035f;
		RTIndirectOcclusionBFConsts.SigmaN = 48.0f;
		RTIndirectOcclusionBFConsts.SigmaC = 0.0f;
		RTIndirectOcclusionBFConsts.IterationCount = 1;

		// for BF pass only R channel is needed
		RTIndirectOcclusionBFConsts.FilterTempRT0 = GraphicInterface->RequestRenderTexture("RTAO_FilterTempRT0"
			, RTIndirectLightExtent
			, UHTextureFormat::UH_FORMAT_R16F
			, RenderTextureSetting);

		RTIndirectOcclusionBFConsts.FilterTempRT1 = GraphicInterface->RequestRenderTexture("RTAO_FilterTempRT1"
			, RTIndirectLightExtent
			, UHTextureFormat::UH_FORMAT_R16F
			, RenderTextureSetting);

		// sky visibility data, stored in volume based on the scene bound
		if (CurrentScene != nullptr)
		{
			const BoundingBox& SceneBound = CurrentScene->GetSceneBound();

			VkExtent2D VolumeSize;
			VolumeSize.width = static_cast<uint32_t>(std::ceilf(SceneBound.Extents.x * 2.0f));
			VolumeSize.height = static_cast<uint32_t>(std::ceilf(SceneBound.Extents.y * 2.0f));
			const uint32_t VolumeSlices = static_cast<uint32_t>(std::ceilf(SceneBound.Extents.z * 2.0f));

			RenderTextureSetting.bIsVolume = true;
			RenderTextureSetting.NumSlices = VolumeSlices;

			if (VolumeSize.width * VolumeSize.height * VolumeSlices > 0)
			{
				GRTSkyData = GraphicInterface->RequestRenderTexture("RTSkyVisibilityData"
					, VolumeSize
					// xyz: bent sky direciton, w: visibility
					, UHTextureFormat::UH_FORMAT_RGBA16F
					, RenderTextureSetting);

				GRTSkyDiscoverAngle = GraphicInterface->RequestRenderTexture("RTSkyDiscoverAngle"
					, VolumeSize
					// store extra sky discover angle
					, UHTextureFormat::UH_FORMAT_R16F
					, RenderTextureSetting);
			}
		}

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
	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0 || !RTParams.bEnableRTShadow)
	{
		return;
	}

	const VkExtent2D ShadowResultExtent = GRTShadowData->GetExtent();

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Shadow");
	{
		UHGameTimerScope Scope("DispatchRayShadowPass", false);
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingShadow)], "RayTracingShadow");

		// transition output buffer to VK_IMAGE_LAYOUT_GENERAL
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTShadowData, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReceiveLightBits, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.FlushResourceBarrier();

		// bind descriptors and RT states
		// [0] is to bind the shader descriptor, and prevent touching other elements. Not a typo!
		RTDescriptorSets[CurrentFrameRT][0] = RTShadowShader->GetDescriptorSet(CurrentFrameRT);
		RenderBuilder.BindRTDescriptorSet(RTShadowShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
		RenderBuilder.BindRTState(RTShadowShader->GetRTState());

		// trace!
		RenderBuilder.TraceRay(ShadowResultExtent, RTShadowShader->GetRayGenTable(), RTShadowShader->GetMissTable(), RTShadowShader->GetHitGroupTable());
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch RT Soft Shadow");
	{
		UHGameTimerScope Scope("DispatchRTSoftShadow", false);
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::SoftShadowPass)], "RTSoftShadowPass");

		// soft shadow pass after ray tracing
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTShadowData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSoftShadow, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.FlushResourceBarrier();

		RenderBuilder.BindComputeState(RTSoftShadowShader->GetComputeState());
		RenderBuilder.BindDescriptorSetCompute(RTSoftShadowShader->GetPipelineLayout(), RTSoftShadowShader->GetDescriptorSet(CurrentFrameRT));

		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(ShadowResultExtent.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(ShadowResultExtent.height, GThreadGroup2D_Y)
			, 1);

		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSoftShadow, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReceiveLightBits, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();
	}
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

void UHDeferredShadingRenderer::DispatchRaySkyLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RaySkyLightPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingSkyLight)], "RayTracingSkyLight");

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Sky Light");

	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSkyData, VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSkyDiscoverAngle, VK_IMAGE_LAYOUT_GENERAL));
	RenderBuilder.FlushResourceBarrier();

	if (RTParams.bEnableRayTracing && RTInstanceCount > 0 && RTParams.bEnableRTIndirectLighting && RTParams.bEnableSkyLight)
	{
		// [0] is to bind the shader descriptor
		RTDescriptorSets[CurrentFrameRT][0] = RTSkyLightShader->GetDescriptorSet(CurrentFrameRT);
		RenderBuilder.BindRTDescriptorSet(RTSkyLightShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
		RenderBuilder.BindRTState(RTSkyLightShader->GetRTState());

		// trace the ray!
		RenderBuilder.TraceRay(GRTSkyData->GetExtent()
			, RTSkyLightShader->GetRayGenTable()
			, RTSkyLightShader->GetMissTable()
			, RTSkyLightShader->GetHitGroupTable()
			, GRTSkyData->GetImageSlices());
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayIndirectLightPass(UHRenderBuilder& RenderBuilder)
{
	UHGameTimerScope Scope("RayIndirectLightPass", false);
	UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingIndirectLight)], "RayTracingIndirectLight");

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Indirect Light");

	if (RTParams.bEnableRayTracing && RTInstanceCount > 0 && RTParams.bEnableRTIndirectLighting)
	{
		// transition resource to UAV ready
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectDiffuse, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectDiffuseHistory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectOcclusion, VK_IMAGE_LAYOUT_GENERAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectOcclusionHistory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTSkyData, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();

		// [0] is to bind the shader descriptor
		RTDescriptorSets[CurrentFrameRT][0] = RTIndirectLightShader->GetDescriptorSet(CurrentFrameRT);
		RenderBuilder.BindRTDescriptorSet(RTIndirectLightShader->GetPipelineLayout(), RTDescriptorSets[CurrentFrameRT]);
		RenderBuilder.BindRTState(RTIndirectLightShader->GetRTState());

		// trace the ray!
		RenderBuilder.TraceRay(GRTIndirectDiffuse->GetExtent()
			, RTIndirectLightShader->GetRayGenTable()
			, RTIndirectLightShader->GetMissTable()
			, RTIndirectLightShader->GetHitGroupTable());

		// filtering for the indirect diffuse
		{
			// temporal accumulation
			RenderBuilder.BindComputeState(RTDiffuseReprojectionShader->GetComputeState());
			RTDiffuseReprojectionShader->BindParameters(RenderBuilder, GRTIndirectDiffuseHistory, GRTIndirectDiffuse, CurrentFrameRT);

			// setup parameter
			UHRTIndirectReprojectionConstants Consts;
			Consts.Resolution[0] = RTIndirectLightExtent.width;
			Consts.Resolution[1] = RTIndirectLightExtent.height;
			Consts.AlphaMin = 0.05f;
			Consts.AlphaMax = 0.2f;
			RenderBuilder.PushConstant(RTDiffuseReprojectionShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT
				, RTDiffuseReprojectionShader->GetPushConstantRange().size, &Consts);

			// trigger a UAV barrier transition (RayGen -> Compute) and dispatch
			RenderBuilder.ResourceBarrier(GRTIndirectDiffuse, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RTIndirectLightExtent.width, GThreadGroup2D_X)
				, MathHelpers::RoundUpDivide(RTIndirectLightExtent.height, GThreadGroup2D_Y)
				, 1);

			// bilateral filtering for RT indirect diffuse
			DispatchBilateralFilter(RenderBuilder, "Indirect Diffuse Bilateral Filter", GRTIndirectDiffuse, GRTIndirectDiffuse, RTIndirectDiffuseBFConsts);
		}

		// filtering for the sky occlusion
		{
			// temporal accumulation
			RenderBuilder.BindComputeState(RTSkyReprojectionShader->GetComputeState());
			RTSkyReprojectionShader->BindParameters(RenderBuilder, GRTIndirectOcclusionHistory, GRTIndirectOcclusion, CurrentFrameRT);

			// setup parameter
			UHRTIndirectReprojectionConstants Consts;
			Consts.Resolution[0] = RTIndirectLightExtent.width;
			Consts.Resolution[1] = RTIndirectLightExtent.height;
			Consts.AlphaMin = 0.05f;
			Consts.AlphaMax = 0.5f;
			RenderBuilder.PushConstant(RTSkyReprojectionShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT
				, RTSkyReprojectionShader->GetPushConstantRange().size, &Consts);

			// trigger a UAV barrier transition (RayGen -> Compute) and dispatch
			RenderBuilder.ResourceBarrier(GRTIndirectOcclusion, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RTIndirectLightExtent.width, GThreadGroup2D_X)
				, MathHelpers::RoundUpDivide(RTIndirectLightExtent.height, GThreadGroup2D_Y)
				, 1);

			// bilateral filtering for RT indirect occlusion
			DispatchBilateralFilter(RenderBuilder, "Indirect Occlusion Bilateral Filter", GRTIndirectOcclusion, GRTIndirectOcclusion, RTIndirectOcclusionBFConsts);
		}

		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectDiffuse, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectDiffuseHistory, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectOcclusion, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectOcclusionHistory, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();

		// copy indirect occlusion history
		RenderBuilder.CopyTexture(GRTIndirectDiffuse, GRTIndirectDiffuseHistory);
		RenderBuilder.CopyTexture(GRTIndirectOcclusion, GRTIndirectOcclusionHistory);

		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectDiffuse, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTIndirectOcclusion, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
		RenderBuilder.FlushResourceBarrier();
	}

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchRayReflectionPass(UHRenderBuilder& RenderBuilder)
{
	if (!RTParams.bEnableRayTracing || RTInstanceCount == 0 || !RTParams.bEnableRTReflection)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Ray Reflection");
	{
		UHGameTimerScope Scope("DispatchRayReflectionPass", false);
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::RayTracingReflection)], "RayTracingReflection");

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

			// ipsample after the half res tracing
			RenderBuilder.BindComputeState(UpsampleNearest2x2Shader->GetComputeState());
			UpsampleNearest2x2Shader->BindParameters(RenderBuilder, CurrentFrameRT, GRTReflectionResult, Consts);

			// trigger a UAV barrier transition (RayGen -> Compute)
			RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(RenderResolution.width, GThreadGroup2D_X)
				, MathHelpers::RoundUpDivide(RenderResolution.height, GThreadGroup2D_Y)
				, 1);
		}
		else
		{
			RenderBuilder.TraceRay(RenderResolution, RTReflectionShader->GetRayGenTable(), RTReflectionShader->GetMissTable(), RTReflectionShader->GetHitGroupTable());
		}
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());

	// generate mips with custom shader and blur the mips [2, Max]
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), "Dispatch Reflection Blur");
	{
		UHGameTimerScope Scope("DispatchReflectionBlur", false);
		UHGPUTimeQueryScope TimeScope(RenderBuilder.GetCmdList(), GPUTimeQueries[UH_ENUM_VALUE(UHRenderPassTypes::ReflectionBlurPass)], "ReflectionBlurPass");

		RenderBuilder.BindComputeState(RTReflectionMipmapShader->GetComputeState());
		const VkExtent2D ReflectionExtent = GRTReflectionResult->GetExtent();

		for (uint32_t Mdx = 1; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			VkExtent2D MipExtent = ReflectionExtent;
			MipExtent.width >>= Mdx;
			MipExtent.height >>= Mdx;

			UHMipmapConstants Constant;
			Constant.MipWidth = MipExtent.width;
			Constant.MipHeight = MipExtent.height;
			Constant.bUseAlphaWeight = (Mdx < UHRTReflectionShader::ReflectionBlurStartMip + 1) ? 1 : 0;

			// push constant and descriptor in fly
			RenderBuilder.PushConstant(RTReflectionMipmapShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UHMipmapConstants), &Constant);
			RTReflectionMipmapShader->BindTextures(RenderBuilder, GRTReflectionResult, GRTReflectionResult, Mdx, CurrentFrameRT);

			RenderBuilder.ResourceBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, Mdx);
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(MipExtent.width, GThreadGroup2D_X), MathHelpers::RoundUpDivide(MipExtent.height, GThreadGroup2D_Y), 1);
		}

		// Kawase blur all mips after the mip2
		UHKawaseBlurConstants KBConsts{};
		KBConsts.PassCount = 4;
		KBConsts.bUseMipAsTempRT = true;
		KBConsts.StartInputMip = UHRTReflectionShader::ReflectionBlurStartMip;
		KBConsts.StartOutputMip = KBConsts.StartInputMip;
		// downsample only otherwise the mip will be too blurry
		KBConsts.bDownsampleOnly = true;

		for (int32_t Mdx = 0; Mdx < KBConsts.PassCount; Mdx++)
		{
			KBConsts.KawaseTempRT.push_back(GRTReflectionResult);
		}
		DispatchKawaseBlur(RenderBuilder, "Blur Reflection Mips", GRTReflectionResult, GRTReflectionResult, KBConsts);

		for (uint32_t Mdx = 0; Mdx < GRTReflectionResult->GetMipMapCount(); Mdx++)
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GRTReflectionResult, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Mdx));
		}
		RenderBuilder.FlushResourceBarrier();
	}
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}