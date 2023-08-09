#pragma once
#define NOMINMAX

#include <memory>
#include <cstdint>
#include <Windows.h>
#include <string>
#include <filesystem>
#include <wrl.h>
#include <wincodec.h>

// debug define
#define WITH_DEBUG !NDEBUG || WITH_EDITOR

// ship define
#define WITH_SHIP NDEBUG && !WITH_EDITOR

#if WITH_DEBUG
	#define LogMessage( str ) OutputDebugString( str );
#else
	#define LogMessage( str )
#endif

#define ENGINE_NAME "Unheard Engine"
#define ENGINE_NAMEW L"Unheard Engine"

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

template <typename T>
using UniquePtr = std::unique_ptr<T>;

#define MakeUnique std::make_unique
#define StdBind std::bind

#if WITH_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif