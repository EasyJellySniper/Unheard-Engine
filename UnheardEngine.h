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
#include <thread>
#include <mutex>

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
inline std::mutex GLogMutex;

#endif

#define UH_ENUM_VALUE(x) static_cast<int32_t>(x)
#define UH_ENUM_VALUE_U(x) static_cast<uint32_t>(x)

inline void UHE_LOG(std::wstring InString)
{
#if WITH_EDITOR
	LogMessage(InString.c_str());

	std::unique_lock<std::mutex> Lock(GLogMutex);
	GLogBuffer.push_back(UHUtilities::ToStringA(InString));
#endif
}

inline void UHE_LOG(std::string InString)
{
#if WITH_EDITOR
	LogMessage(std::filesystem::path(InString).wstring().c_str());

	std::unique_lock<std::mutex> Lock(GLogMutex);
	GLogBuffer.push_back(InString);
#endif
}

template<typename T>
inline void ClearContainer(std::vector<T>& InVector)
{
	for (auto& Element : InVector)
	{
		UH_SAFE_RELEASE(Element);
	}
	InVector.clear();
}

template<typename T1, typename T2>
inline void ClearContainer(std::unordered_map<T1, T2>& InMap)
{
	for (auto& Element : InMap)
	{
		UH_SAFE_RELEASE(Element.second);
	}
	InMap.clear();
}