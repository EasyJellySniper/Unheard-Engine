#pragma once
#include <cstdint>
#include <string>

#if _WIN32
#include <Windows.h>
#include "resource.h"
#elif __linux__
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#endif

// multiple clients management in UnheardEngine
class UHClient
{
public:
	UHClient()
		: NativeInstance(nullptr)
		, NativeWindow(nullptr)
		, bIsQuit(false)
	{

	}

	void SetClientInstance(void* InInstance)
	{
		NativeInstance = InInstance;
	}

	void SetClientWindow(void* InWindow)
	{
		NativeWindow = InWindow;
	}

	void* GetNativeInstance() const
	{
		return NativeInstance;
	}

	void* GetNativeWindow() const
	{
		return NativeWindow;
	}

	void SetWindowPosition(const int32_t Width, const int32_t Height);
	void GetWindowSize(int32_t& OutWidth, int32_t& OutHeight) const;
	void SetWindowStyle(const bool bIsFullscreen, const int32_t Width, const int32_t Height);
	void SetWindowCaption(const std::string InCaption);

	bool IsWindowMinimized();
	bool IsQuit();
	void ProcessEvents();

	int32_t GetDisplayFrequency() const;

private:
	void* NativeInstance;
	void* NativeWindow;
	bool bIsQuit;
};