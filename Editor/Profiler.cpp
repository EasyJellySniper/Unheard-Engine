#include "Profiler.h"

// debug only class
#if WITH_DEBUG

UHProfiler::UHProfiler()
	: FrameCounter(0)
	, TimeElapsed(0.0f)
	, FPS(0.0f)
{

}

float UHProfiler::CalculateFPS(UHGameTimer* Timer)
{
	if (Timer == nullptr)
	{
		return 0.0f;
	}

	FrameCounter++;

	// Compute averages over one second period
	if (Timer->GetTotalTime() - TimeElapsed >= 1.0f)
	{
		FPS = (float)FrameCounter;

		// Reset for next average
		FrameCounter = 0;
		TimeElapsed += 1.0f;
	}

	return FPS;
}

#endif