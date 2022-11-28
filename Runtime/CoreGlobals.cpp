#include "CoreGlobals.h"

uint32_t GFrameNumber = 0;

// always true and will be checked again during GFX initialization
bool GEnableRayTracing = true;

#if WITH_DEBUG
bool GEnableGPUTiming = true;
#endif

// shared use GPU memory
std::unique_ptr<UHGPUMemory> GGPUMeshBufferMemory = nullptr;
std::unique_ptr<UHGPUMemory> GGPUImageMemory = nullptr;