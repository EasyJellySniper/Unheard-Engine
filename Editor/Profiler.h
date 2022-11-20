#pragma once
#include "../UnheardEngine.h"
#include "../Runtime/Engine/GameTimer.h"

#if WITH_DEBUG

// Debug only Profiler class
class UHProfiler
{
public:
	UHProfiler();

	float CalculateFPS(UHGameTimer* Timer);

private:
	int32_t FrameCounter;
	float TimeElapsed;
	float FPS;
};

#endif

