#pragma once
#include "Platform/Platform.h"

// @TODO: make linux possible to run engine
#if _WIN32
#include "Runtime/Engine/Engine.h"
#endif

// UnheardEngine application, usually called by the program entry point.
class UHApplication
{
public:
	UHApplication();
	int32_t Run();

	// @TODO: make linux possible to run engine
#if _WIN32
	UHEngine* GetEngine();
#endif

private:
	UniquePtr<UHPlatform> Platform;
	// @TODO: make linux possible to run engine
#if _WIN32
	UniquePtr<UHEngine> Engine;
#endif
};