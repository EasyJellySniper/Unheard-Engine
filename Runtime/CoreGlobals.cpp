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

std::thread::id GMainThreadID;