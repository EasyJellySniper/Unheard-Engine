#pragma once
#include <cstdint>
#include "../UnheardEngine.h"

// header for global shared definitions, only define things here if necessary
extern uint32_t GFrameNumber;

#if WITH_EDITOR
extern bool GEnableGPUTiming;
#endif

extern const uint32_t GMainThreadAffinity;
extern const uint32_t GRenderThreadAffinity;
extern const uint32_t GWorkerThreadAffinity;
static const uint32_t GMaxWorkerThreads = 16;

extern std::thread::id GMainThreadID;
extern bool GIsEditor;
extern bool GIsShipping;