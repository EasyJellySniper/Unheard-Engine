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
extern const uint32_t GMaxParallelSubmitters;

extern std::thread::id GMainThreadID;
extern std::thread::id GRenderThreadID;
extern std::vector<std::thread::id> GWorkerThreadIDs;
extern thread_local std::thread::id GCurrentThreadID;

extern bool GIsEditor;
extern bool GIsShipping;

extern bool IsInGameThread();
extern bool IsInRenderThread();
extern bool IsInWorkerThread();