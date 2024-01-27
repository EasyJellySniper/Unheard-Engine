#pragma once
#include "../UnheardEngine.h"
#include "../Runtime/Engine/GameTimer.h"
#include <unordered_map>
#include "../Runtime/Renderer/RenderingTypes.h"

struct UHStatistics
{
public:
	UHStatistics()
		: EngineUpdateTime(0)
		, RenderThreadTime(0)
		, TotalTime(0)
		, FPS(0)
		, DrawCallCount(0)
		, PSOCount(0)
		, ShaderCount(0)
		, RTCount(0)
		, SamplerCount(0)
		, TextureCount(0)
		, TextureCubeCount(0)
		, MateralCount(0)
	{
		for (int32_t Idx = 0; Idx < UHRenderPassTypes::UHRenderPassMax; Idx++)
		{
			GPUTimes[Idx] = 0.0f;
		}
	}

	void SetGPUTimes(float* InTime)
	{
		for (int32_t Idx = 0; Idx < UHRenderPassTypes::UHRenderPassMax; Idx++)
		{
			GPUTimes[Idx] = InTime[Idx];
		}
	}

	float EngineUpdateTime;
	float RenderThreadTime;
	float TotalTime;
	float FPS;
	float GPUTimes[UHRenderPassTypes::UHRenderPassMax];

	int32_t DrawCallCount;
	int32_t PSOCount;
	int32_t ShaderCount;
	int32_t RTCount;
	int32_t SamplerCount;
	int32_t TextureCount;
	int32_t TextureCubeCount;
	int32_t MateralCount;
};

// Profiler class
class UHProfiler
{
public:
	UHProfiler();
	UHProfiler(UHGameTimer* InTimer);
	UHStatistics& GetStatistics();

	void Begin();
	void End();
	float GetDiff();

private:
	UHGameTimer* Timer;
	float TimeElapsedThisFrame;

	UHStatistics Statistics;
	int64_t BeginTime;
	int64_t EndTime;
};

// profiler scope, kick off a profiler in ctor and finish in dtor
class UHProfilerScope
{
public:
	UHProfilerScope(UHProfiler* InProfiler);
	~UHProfilerScope();

private:
	UHProfiler* Profiler;
};

