#pragma once
#include <cstdint>

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

// rendering settings
struct UHRenderingSettings
{
public:
	UHRenderingSettings()
		: RenderWidth(1920)
		, RenderHeight(1080)
		, bTemporalAA(true)
		, bEnableRayTracing(true)
		, bEnableRayTracingOcclusionTest(true)
		, bEnableGPULabeling(true)
		, bEnableLayerValidation(true)
		, bEnableGPUTiming(true)
		, bEnableDepthPrePass(false)
		, bEnableDrawBundles(true)
		, RTDirectionalShadowQuality(0)
	{

	}

	int32_t RenderWidth;
	int32_t RenderHeight;
	bool bTemporalAA;
	bool bEnableRayTracing;
	bool bEnableRayTracingOcclusionTest;
	bool bEnableGPULabeling;
	bool bEnableLayerValidation;
	bool bEnableGPUTiming;
	bool bEnableDepthPrePass;
	bool bEnableDrawBundles;
	int32_t RTDirectionalShadowQuality;
};