#include "CoreGlobals.h"

uint32_t GFrameNumber = 0;

#if WITH_EDITOR
bool GEnableGPUTiming = true;
#endif

// the starting core of threads
uint32_t GMainThreadAffinity = 0;
uint32_t GRenderThreadAffinity = 1;
uint32_t GWorkerThreadAffinity = 2;