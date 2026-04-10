#include "Profiler.h"

UHProfiler::UHProfiler()
	: UHProfiler(nullptr)
{
}

UHProfiler::UHProfiler(UHGameTimer* InTimer)
	: Timer(InTimer)
	, TimeElapsedThisFrame(0.0f)
	, Statistics(UHStatistics())
	, BeginTime(UHClock::time_point())
	, EndTime(UHClock::time_point())
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
	return std::chrono::duration<float>(EndTime - BeginTime).count();
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