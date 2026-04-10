#pragma once
#include "Platform/Platform.h"
#include "Runtime/Engine/Engine.h"

// UnheardEngine application, usually called by the program entry point.
class UHApplication
{
public:
	UHApplication();
	int32_t Run();
	UHEngine* GetEngine();

private:
	UniquePtr<UHPlatform> Platform;
	UniquePtr<UHEngine> Engine;
};