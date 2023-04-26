#pragma once
#include "../UnheardEngine.h"
#include "../Runtime/Engine/GameTimer.h"

#if WITH_DEBUG
#include <unordered_map>
#include "../Runtime/Renderer/RenderingTypes.h"

struct UHStatistics
{
public:
	UHStatistics()
		: MainThreadTime(0)
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

	float MainThreadTime;
	float RenderThreadTime;
	float TotalTime;
	float FPS;
	std::array<float, UHRenderPassTypes::UHRenderPassMax> GPUTimes;

	int32_t DrawCallCount;
	int32_t PSOCount;
	int32_t ShaderCount;
	int32_t RTCount;
	int32_t SamplerCount;
	int32_t TextureCount;
	int32_t TextureCubeCount;
	int32_t MateralCount;
};

// Debug only Profiler class
class UHProfiler
{
public:
	UHProfiler();
	UHProfiler(UHGameTimer* InTimer);

	float CalculateFPS();
	UHStatistics& GetStatistics();

	void Begin();
	void End();
	float GetDiff();

private:
	UHGameTimer* Timer;
	int32_t FrameCounter;
	float TimeElapsedThisFrame;
	float FPS;

	UHStatistics Statistics;
	int64_t BeginTime;
	int64_t EndTime;
};

#endif

