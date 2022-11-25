#include "Profiler.h"

// debug only class
#if WITH_DEBUG

UHProfiler::UHProfiler()
	: UHProfiler(nullptr)
{
}

UHProfiler::UHProfiler(UHGameTimer* InTimer)
	: Timer(InTimer)
	, FrameCounter(0)
	, TimeElapsedThisFrame(0.0f)
	, FPS(0.0f)
	, Statistics(UHStatistics())
	, BeginTime(0)
	, EndTime(0)
{

}

float UHProfiler::CalculateFPS()
{
	if (Timer == nullptr)
	{
		return 0.0f;
	}

	FrameCounter++;

	// Compute averages over one second period
	if (Timer->GetTotalTime() - TimeElapsedThisFrame >= 1.0f)
	{
		FPS = (float)FrameCounter;

		// Reset for next average
		FrameCounter = 0;
		TimeElapsedThisFrame += 1.0f;
	}

	return FPS;
}

UHStatistics& UHProfiler::GetStatistics()
{
	return Statistics;
}

void UHProfiler::Begin()
{
	BeginTime = Timer->GetTime();
}

void UHProfiler::End()
{
	EndTime = Timer->GetTime();
}

float UHProfiler::GetDiff()
{
	return static_cast<float>((EndTime - BeginTime) * Timer->GetSecondsPerCount());
}

#endif