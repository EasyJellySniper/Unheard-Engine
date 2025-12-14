#pragma once
#include "../../UnheardEngine.h"

// file to define all kinds of setting

// presentation settings
struct UHPresentationSettings
{
public:
	UHPresentationSettings()
		: WindowWidth(0)
		, WindowHeight(0)
		, bVsync(false)
		, bFullScreen(false)
	{

	}

	int32_t WindowWidth;
	int32_t WindowHeight;
	bool bVsync;
	bool bFullScreen;
};

// engine settings
struct UHEngineSettings
{
public:
	UHEngineSettings()
		: DefaultCameraMoveSpeed(10)
		, MouseRotationSpeed(30)
		, ForwardKey('w')
		, BackKey('s')
		, LeftKey('a')
		, RightKey('d')
		, DownKey('q')
		, UpKey('e')
		, FPSLimit(0.0f)
		, MeshBufferMemoryBudgetMB(512.0f)
		, ImageMemoryBudgetMB(1024.0f)
	{

	}

	float DefaultCameraMoveSpeed;
	float MouseRotationSpeed;
	char ForwardKey;
	char BackKey;
	char LeftKey;
	char RightKey;
	char DownKey;
	char UpKey;
	float FPSLimit;
	float MeshBufferMemoryBudgetMB;
	float ImageMemoryBudgetMB;
};

enum class UHRTShadowQuality
{
	RTShadow_Full,
	RTShadow_Half
};

enum class UHRTReflectionQuality
{
	RTReflection_Full,
	RTReflection_Half
};

// rendering settings
struct UHRenderingSettings
{
public:
	UHRenderingSettings()
		: RenderWidth(1920)
		, RenderHeight(1080)
		, bTemporalAA(true)
		, bEnableRayTracing(true)
		, bEnableGPULabeling(false)
		, bEnableLayerValidation(false)
		, bEnableGPUTiming(false)
		, bEnableDepthPrePass(true)
		, ParallelSubmitters(8)
		, RTCullingRadius(100.0f)
		, RTShadowQuality(UH_ENUM_VALUE(UHRTShadowQuality::RTShadow_Half))
		, RTShadowTMax(100.0f)
		, RTReflectionQuality(UH_ENUM_VALUE(UHRTReflectionQuality::RTReflection_Half))
		, RTReflectionTMax(100)
		, RTReflectionSmoothCutoff(0.5f)
		, FinalReflectionStrength(0.25f)
		, bEnableAsyncCompute(true)
		, bEnableHDR(false)
		, bEnableHardwareOcclusion(true)
		, OcclusionTriangleThreshold(500)
		, GammaCorrection(2.2f)
		, HDRWhitePaperNits(200.0f)
		, HDRContrast(1.3f)
		, PCSSKernal(2)
		, PCSSMinPenumbra(1.5f)
		, PCSSMaxPenumbra(10.0f)
		, PCSSBlockerDistScale(0.02f)
		, bDenoiseRayTracing(true)
		, SelectedGpuName("")
		, bEnableRTShadow(true)
		, bEnableRTReflection(true)
		, bEnableRTIndirectLighting(false)
		, RTIndirectLightScale(0.75f)
		, RTIndirectLightFadeDistance(20.0f)
		, RTIndirectTMax(50.0f)
		, OcclusionStartDistance(0.0f)
		, OcclusionEndDistance(15.0f)
		, IndirectRayOffsetScale(0.5f)
	{

	}

	int32_t RenderWidth;
	int32_t RenderHeight;
	bool bTemporalAA;
	bool bEnableRayTracing;
	bool bEnableGPULabeling;
	bool bEnableLayerValidation;
	bool bEnableGPUTiming;
	bool bEnableDepthPrePass;
	int32_t ParallelSubmitters;

	// common gamma settings
	float GammaCorrection;

	// RT common
	float RTCullingRadius;
	bool bDenoiseRayTracing;

	// RT direct light
	int32_t RTShadowQuality;
	float RTShadowTMax;
	bool bEnableRTShadow;

	// RT reflections
	int32_t RTReflectionQuality;
	float RTReflectionTMax;
	float RTReflectionSmoothCutoff;
	float FinalReflectionStrength;
	bool bEnableRTReflection;

	// RT indirect lighting
	bool bEnableRTIndirectLighting;
	float RTIndirectLightScale;
	float RTIndirectLightFadeDistance;
	float RTIndirectTMax;
	float OcclusionStartDistance;
	float OcclusionEndDistance;
	float IndirectRayOffsetScale;

	bool bEnableAsyncCompute;
	bool bEnableHardwareOcclusion;
	int32_t OcclusionTriangleThreshold;

	// HDR settings
	bool bEnableHDR;
	float HDRWhitePaperNits;
	float HDRContrast;

	// soft shadow settings
	int32_t PCSSKernal;
	float PCSSMinPenumbra;
	float PCSSMaxPenumbra;
	float PCSSBlockerDistScale;

	// rendering device selection
	std::string SelectedGpuName;
};