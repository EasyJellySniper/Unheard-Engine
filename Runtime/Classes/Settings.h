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
		, FPSLimit(60.0f)
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
	RTReflection_FullNative,
	RTReflection_FullTemporal,
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
		, bEnableGPULabeling(true)
		, bEnableLayerValidation(true)
		, bEnableGPUTiming(true)
		, bEnableDepthPrePass(false)
		, ParallelThreads(0)
		, RTCullingRadius(100.0f)
		, RTShadowQuality(UH_ENUM_VALUE(UHRTShadowQuality::RTShadow_Half))
		, RTShadowTMax(100.0f)
		, RTReflectionQuality(UH_ENUM_VALUE(UHRTReflectionQuality::RTReflection_FullTemporal))
		, RTReflectionTMax(100)
		, RTReflectionSmoothCutoff(0.5f)
		, FinalReflectionStrength(0.75f)
		, bEnableAsyncCompute(false)
		, bEnableHDR(false)
		, bEnableHardwareOcclusion(true)
		, OcclusionTriangleThreshold(5000)
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
	int32_t ParallelThreads;

	// RT common
	float RTCullingRadius;

	// RT shadows
	int32_t RTShadowQuality;
	float RTShadowTMax;

	// RT reflections
	int32_t RTReflectionQuality;
	float RTReflectionTMax;
	float RTReflectionSmoothCutoff;
	float FinalReflectionStrength;

	bool bEnableAsyncCompute;
	bool bEnableHDR;
	bool bEnableHardwareOcclusion;
	int32_t OcclusionTriangleThreshold;
};