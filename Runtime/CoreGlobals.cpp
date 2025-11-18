#include "CoreGlobals.h"

uint32_t GFrameNumber = 0;

#if WITH_EDITOR
bool GEnableGPUTiming = true;
bool GIsEditor = true;
bool GIsShipping = false;
#else
bool GIsEditor = false;
bool GIsShipping = true;
#endif

// the starting core of threads
const uint32_t GMainThreadAffinity = 0;
const uint32_t GRenderThreadAffinity = 1;
const uint32_t GWorkerThreadAffinity = 2;
const uint32_t GMaxParallelSubmitters = 8;

std::thread::id GMainThreadID;
std::thread::id GRenderThreadID;
std::vector<std::thread::id> GWorkerThreadIDs;
thread_local std::thread::id GCurrentThreadID;

bool IsInGameThread()
{
	return GCurrentThreadID == GMainThreadID;
}

bool IsInRenderThread()
{
	return GCurrentThreadID == GRenderThreadID;
}

bool IsInWorkerThread()
{
	for (size_t Idx = 0; Idx < GWorkerThreadIDs.size(); Idx++)
	{
		if (GCurrentThreadID == GWorkerThreadIDs[Idx])
		{
			return true;
		}
	}

	return false;
}