#include "Profiler.h"

UHProfiler::UHProfiler()
	: UHProfiler(nullptr)
{
}

UHProfiler::UHProfiler(UHGameTimer* InTimer)
	: Timer(InTimer)
	, TimeElapsedThisFrame(0.0f)
	, Statistics(UHStatistics())
	, BeginTime(0)
	, EndTime(0)
{

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

UHProfilerScope::UHProfilerScope(UHProfiler* InProfiler)
	: Profiler(InProfiler)
{
#if WITH_EDITOR
	Profiler->Begin();
#endif
}

UHProfilerScope::~UHProfilerScope()
{
#if WITH_EDITOR
	Profiler->End();
#endif
}