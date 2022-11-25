#pragma once
#include <cstdint>
#include "../UnheardEngine.h"

// header for global shared definitions, only define things here if necessary
extern uint32_t GFrameNumber;
extern bool GEnableRayTracing;

#if WITH_DEBUG
extern bool GEnableGPUTiming;
#endif