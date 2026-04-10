#pragma once
// vulkan header for cross-platform purpose
// might also contain platform-based implementation

#if _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif __linux__
#define VK_USE_PLATFORM_XCB_KHR
// some features might be hidden due to driver support on Linux, define VK_ENABLE_BETA_EXTENSIONS to enable them 
#define VK_ENABLE_BETA_EXTENSIONS
#endif

#include <vulkan/vulkan.h>