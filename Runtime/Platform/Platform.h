#pragma once

// UnheardEngine platform class, for multiple platform supports.
// The class is not virtualized yet, implementations depend on project setup.
// E.g. PlatformWindows.cpp if built from Visual Studio. Or PlatformLinux.cpp if built by CMake.
class UHPlatform
{
public:
	static int PlatformRun();
};