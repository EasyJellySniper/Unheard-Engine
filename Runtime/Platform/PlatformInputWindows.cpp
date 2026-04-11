#include "PlatformInput.h"
#include "../../framework.h"

#if _WIN32

#include <hidusage.h>

bool UHPlatformInput::InitInput()
{
	enum InputDeviceType
	{
		Mouse, Keyboard
	};

	RAWINPUTDEVICE RawInputDevice[2];
	RawInputDevice[InputDeviceType::Mouse].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawInputDevice[InputDeviceType::Mouse].usUsage = HID_USAGE_GENERIC_MOUSE;
	RawInputDevice[InputDeviceType::Mouse].dwFlags = NULL;   // adds HID mouse and also ignores legacy mouse messages
	RawInputDevice[InputDeviceType::Mouse].hwndTarget = 0;

	RawInputDevice[InputDeviceType::Keyboard].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawInputDevice[InputDeviceType::Keyboard].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	RawInputDevice[InputDeviceType::Keyboard].dwFlags = NULL;   // adds HID keyboard and also ignores legacy keyboard messages
	RawInputDevice[InputDeviceType::Keyboard].hwndTarget = 0;

	// tell system to send input message to UHE client
	return RegisterRawInputDevices(RawInputDevice, 2, sizeof(RawInputDevice[0]));
}

void UHPlatformInput::ParseInputData(void* InData)
{
	// parse input data of this event, this should be called in Wndroc when WM_INPUT is occurred
	LPARAM LParam = (LPARAM)InData;
	UINT DataSize = sizeof(RAWINPUT);

	RAWINPUT RawInputData;
	GetRawInputData((HRAWINPUT)LParam,
		RID_INPUT,
		&RawInputData,
		&DataSize,
		sizeof(RAWINPUTHEADER));

	if (WM_LBUTTONDOWN == LParam)
	{
		bIsLeftMousePressed = true;
	}
	if (WM_RBUTTONDOWN == LParam)
	{
		bIsRightMousePressed = true;
	}

	if (RawInputData.header.dwType == RIM_TYPEMOUSE)
	{
		if (RawInputData.data.mouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_DOWN)
		{
			bIsLeftMousePressed = true;
		}
		else if (RawInputData.data.mouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_UP)
		{
			bIsLeftMousePressed = false;
		}

		if (RawInputData.data.mouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_DOWN)
		{
			bIsRightMousePressed = true;
		}
		else if (RawInputData.data.mouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_UP)
		{
			bIsRightMousePressed = false;
		}

		LastMouseMovementX = RawInputData.data.mouse.lLastX;
		LastMouseMovementY = RawInputData.data.mouse.lLastY;
	}
	else if (RawInputData.header.dwType == RIM_TYPEKEYBOARD)
	{
		if (RawInputData.data.keyboard.Message == WM_KEYDOWN || RawInputData.data.keyboard.Message == WM_SYSKEYDOWN)
		{
			bCurrentKeyState[RawInputData.data.keyboard.VKey] = true;
		}
		else if (RawInputData.data.keyboard.Message == WM_KEYUP || RawInputData.data.keyboard.Message == WM_SYSKEYUP)
		{
			bCurrentKeyState[RawInputData.data.keyboard.VKey] = false;
		}
	}
}

void UHPlatformInput::GetMousePosition(long& X, long& Y) const
{
	POINT MousePos;
	GetCursorPos(&MousePos);
	X = MousePos.x;
	Y = MousePos.y;
}

#endif