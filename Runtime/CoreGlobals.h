#pragma once
#include <cstdint>
#include "../UnheardEngine.h"
#include <memory>

// header for global shared definitions, only define things here if necessary
extern uint32_t GFrameNumber;

#if WITH_DEBUG
extern bool GEnableGPUTiming;
#endif