#include "Client.h"

#if _WIN32
#include <Windows.h>
#include "Runtime/CoreGlobals.h"
#include "resource.h"

void UHClient::SetWindowPosition(const int32_t Width, const int32_t Height)
{
	SetWindowPos((HWND)NativeWindow, nullptr, 0, 0, Width, Height, 0);
}

void UHClient::GetWindowSize(int32_t& OutWidth, int32_t& OutHeight) const
{
	RECT Rect;
	if (GetWindowRect((HWND)NativeWindow, &Rect))
	{
		OutWidth = Rect.right - Rect.left;
		OutHeight = Rect.bottom - Rect.top;
	}
}

void UHClient::SetWindowStyle(const bool bIsFullscreen, const int32_t Width, const int32_t Height)
{
	if (bIsFullscreen)
	{
		// simply use desktop size as fullscreen size
		HWND DesktopWindow = GetDesktopWindow();
		RECT Rect;
		int32_t DesktopWidth = 0;
		int32_t DesktopHeight = 0;

		if (GetWindowRect(DesktopWindow, &Rect))
		{
			DesktopWidth = Rect.right - Rect.left;
			DesktopHeight = Rect.bottom - Rect.top;
		}

		HWND ClientWindow = (HWND)NativeWindow;
		SetWindowLongPtr(ClientWindow, GWL_STYLE, WS_POPUP);
		SetWindowPos(ClientWindow, nullptr, 0, 0, DesktopWidth, DesktopHeight, 0);
		SetMenu(ClientWindow, nullptr);
		ShowWindow(ClientWindow, SW_SHOW);
	}
	else
	{
		// recover to window
		HINSTANCE ClientInstance = (HINSTANCE)NativeInstance;
		HWND ClientWindow = (HWND)NativeWindow;
		SetWindowLongPtr(ClientWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(ClientWindow, nullptr, 0, 0, Width, Height, 0);

		if (GIsEditor)
		{
			// recover menu and icon with debug mode only
			HICON Icon = LoadIcon(ClientInstance, MAKEINTRESOURCE(IDI_UNHEARDENGINE));
			SendMessage(ClientWindow, WM_SETICON, ICON_SMALL, (LPARAM)Icon);

			HMENU Menu = LoadMenu(ClientInstance, MAKEINTRESOURCE(IDC_UNHEARDENGINE));
			SetMenu(ClientWindow, Menu);
		}

		ShowWindow(ClientWindow, SW_SHOW);
	}
}

void UHClient::SetWindowCaption(const std::string InCaption)
{
	SetWindowTextA((HWND)NativeWindow, InCaption.c_str());
}

bool UHClient::IsWindowMinimized()
{
	return !!IsIconic((HWND)NativeWindow);
}

bool UHClient::IsQuit()
{
	return bIsQuit;
}

bool UHClient::ProcessEvents()
{
	bool bHasMessage = false;
	MSG msg = {};
	if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		bHasMessage = true;
	}

	bIsQuit = (msg.message == WM_QUIT);
	return bHasMessage;
}
#endif