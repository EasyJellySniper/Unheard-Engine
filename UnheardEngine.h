#pragma once
#define NOMINMAX

#include <memory>
#include <cstdint>
#include <Windows.h>
#include <string>
#include <filesystem>
#include <wrl.h>
#include <wincodec.h>
#include "Runtime/Classes/Types.h"

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

inline void UHE_LOG(std::wstring InString)
{
	LogMessage(InString.c_str());
}

inline void UHE_LOG(std::string InString)
{
	LogMessage(std::filesystem::path(InString).wstring().c_str());
}

#if WITH_EDITOR
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif