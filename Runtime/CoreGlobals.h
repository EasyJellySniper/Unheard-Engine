#pragma once
#include <cstdint>
#include <vector>
#include <thread>
#include <assert.h>

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

extern float GEpsilon;
extern float GWorldMax;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

#define MakeUnique std::make_unique
#define StdBind std::bind
#define UHINDEXNONE -1

#define ENGINE_NAME "Unheard Engine"
#define ENGINE_NAMEW L"Unheard Engine"
#define ENGINE_NAME_NONE "UHE_None"