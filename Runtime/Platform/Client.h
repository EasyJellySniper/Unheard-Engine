#pragma once
#include <cstdint>
#include <string>

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
	bool ProcessEvents();

private:
	void* NativeInstance;
	void* NativeWindow;
	bool bIsQuit;
};