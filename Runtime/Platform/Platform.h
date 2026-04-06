#pragma once
#include "Runtime/CoreGlobals.h"
#include "Client.h"

// UnheardEngine platform class, for multiple platform supports.
// The class is not virtualized yet, implementations depend on project setup.
// E.g. PlatformWindows.cpp if built from Visual Studio. Or PlatformLinux.cpp if built by CMake.
class UHApplication;
class UHPlatform
{
public:
	UHPlatform()
	{

	}

	bool Initialize(UHApplication* InApp);
	void Shutdown();

	UHClient* GetClient()
	{
		return Client.get();
	}

private:
	UniquePtr<UHClient> Client;
};