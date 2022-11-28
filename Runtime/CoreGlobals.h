#pragma once
#include <cstdint>
#include "../UnheardEngine.h"
#include "Classes/GPUMemory.h"
#include <memory>

// header for global shared definitions, only define things here if necessary
extern uint32_t GFrameNumber;
extern bool GEnableRayTracing;

#if WITH_DEBUG
extern bool GEnableGPUTiming;
#endif

extern std::unique_ptr<UHGPUMemory> GGPUMeshBufferMemory;
extern std::unique_ptr<UHGPUMemory> GGPUImageMemory;