#pragma once
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <memory>
#include <cstdint>
#include <Windows.h>
#include <string>
#include <filesystem>
#include <wrl.h>
#include <wincodec.h>
#include "Runtime/Classes/Types.h"
#include "Runtime/Classes/Utility.h"

#if WITH_EDITOR
	#define LogMessage( str ) OutputDebugString( str );
#else
	#define LogMessage( str )
#endif

#define ENGINE_NAME "Unheard Engine"
#define ENGINE_NAMEW L"Unheard Engine"
#define ENGINE_NAME_NONE "UHE_None"

// macro for safe release UH objects
#define UH_SAFE_RELEASE(x) if (x) x->Release();

#if WITH_EDITOR
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"
#include "ThirdParty/ImGui/imgui_impl_vulkan.h"

inline std::vector<std::string> GLogBuffer;

#endif

inline void UHE_LOG(std::wstring InString)
{
#if WITH_EDITOR
	LogMessage(InString.c_str());
	GLogBuffer.push_back(UHUtilities::ToStringA(InString));
#endif
}

inline void UHE_LOG(std::string InString)
{
#if WITH_EDITOR
	LogMessage(std::filesystem::path(InString).wstring().c_str());
	GLogBuffer.push_back(InString);
#endif
}