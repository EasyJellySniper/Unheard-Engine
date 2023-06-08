#pragma once
#include <cstdint>
#include "../UnheardEngine.h"

// header for global shared definitions, only define things here if necessary
extern uint32_t GFrameNumber;

#if WITH_DEBUG
extern bool GEnableGPUTiming;
#endif

extern uint32_t GMainThreadAffinity;
extern uint32_t GRenderThreadAffinity;
extern uint32_t GWorkerThreadAffinity;