#pragma once
#define NOMINMAX

#include <memory>
#include <cstdint>
#include <Windows.h>
#include <string>

// debug define
#define WITH_DEBUG !NDEBUG

// ship define
#define WITH_SHIP NDEBUG

#ifdef WITH_DEBUG
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