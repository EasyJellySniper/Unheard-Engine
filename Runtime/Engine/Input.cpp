#include "Input.h"
#include <hidusage.h>

UHRawInput::UHRawInput()
	: RawInputDevice{}
	, RawInputData()
	, bPreviousKeyState{}
	, bCurrentKeyState{}
	, bIsLeftMousePressed(false)
	, bIsRightMousePressed(false)
	, bPreviousLeftMousePressed(false)
	, bPreviousRightMousePressed(false)
{

}

bool UHRawInput::InitRawInput()
{
	RawInputDevice[InputDeviceType::Mouse].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawInputDevice[InputDeviceType::Mouse].usUsage = HID_USAGE_GENERIC_MOUSE;
	RawInputDevice[InputDeviceType::Mouse].dwFlags = NULL;   // adds HID mouse and also ignores legacy mouse messages
	RawInputDevice[InputDeviceType::Mouse].hwndTarget = 0;

	RawInputDevice[InputDeviceType::Keyboard].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RawInputDevice[InputDeviceType::Keyboard].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	RawInputDevice[InputDeviceType::Keyboard].dwFlags = NULL;   // adds HID keyboard and also ignores legacy keyboard messages
	RawInputDevice[InputDeviceType::Keyboard].hwndTarget = 0;

	return RegisterRawInputDevices(RawInputDevice, 2, sizeof(RawInputDevice[0]));
}

void UHRawInput::ParseInputData(LPARAM LParam)
{
	// parse input data of this event, this should be called in Wndroc when WM_INPUT is occurred
	UINT DataSize = sizeof(RAWINPUT);
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

void UHRawInput::ResetMouseData()
{
	RawInputData.data.mouse.lLastX = 0;
	RawInputData.data.mouse.lLastY = 0;
	LastMousePos.x = 0;
	LastMousePos.y = 0;
}

void UHRawInput::ResetMouseState()
{
	bIsLeftMousePressed = false;
	bPreviousLeftMousePressed = false;
	bIsRightMousePressed = false;
	bPreviousRightMousePressed = false;
	RawInputData.data.mouse = RAWMOUSE();
}

void UHRawInput::SetLeftMousePressed(bool bFlag)
{
	bIsLeftMousePressed = bFlag;
}

void UHRawInput::SetRightMousePressed(bool bFlag)
{
	bIsRightMousePressed = bFlag;
}

void UHRawInput::GetMouseDelta(uint32_t& X, uint32_t& Y) const
{
	POINT MousePos;
	GetCursorPos(&MousePos);

	X = MousePos.x - LastMousePos.x;
	Y = MousePos.y - LastMousePos.y;
}

RAWMOUSE UHRawInput::GetMouseData() const
{
	return RawInputData.data.mouse;
}

void UHRawInput::CacheKeyStates()
{
	// cache key states of current frame, this should be called at the end of update functions, or at least before any input checking
	memcpy_s(bPreviousKeyState, sizeof(bool) * ARRAYSIZE(bPreviousKeyState), bCurrentKeyState, sizeof(bool) * ARRAYSIZE(bCurrentKeyState));
	bPreviousLeftMousePressed = bIsLeftMousePressed;
	bPreviousRightMousePressed = bIsRightMousePressed;

	// cache last mouse pos too
	POINT MousePos;
	GetCursorPos(&MousePos);

	LastMousePos = MousePos;
}

bool UHRawInput::IsKeyHold(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyHold((int32_t)CharKey);
}

bool UHRawInput::IsKeyDown(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyDown((int32_t)CharKey);
}

bool UHRawInput::IsKeyUp(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyUp((int32_t)CharKey);
}

bool UHRawInput::IsKeyHold(int32_t VkKey) const
{
	return bCurrentKeyState[VkKey] && bPreviousKeyState[VkKey];
}

bool UHRawInput::IsKeyDown(int32_t VkKey) const
{
	return bCurrentKeyState[VkKey] && !bPreviousKeyState[VkKey];
}

bool UHRawInput::IsKeyUp(int32_t VkKey) const
{
	return !bCurrentKeyState[VkKey] && bPreviousKeyState[VkKey];
}

bool UHRawInput::IsLeftMouseHold() const
{
	return bIsLeftMousePressed && bPreviousLeftMousePressed;
}

bool UHRawInput::IsRightMouseHold() const
{
	return bIsRightMousePressed && bPreviousRightMousePressed;
}

bool UHRawInput::IsLeftMouseDown() const
{
	return bIsLeftMousePressed && !bPreviousLeftMousePressed;
}

bool UHRawInput::IsRightMouseDown() const
{
	return bIsRightMousePressed && !bPreviousRightMousePressed;
}

bool UHRawInput::IsLeftMouseUp() const
{
	return !bIsLeftMousePressed && bPreviousLeftMousePressed;
}

bool UHRawInput::IsRightMouseUp() const
{
	return !bIsRightMousePressed && bPreviousRightMousePressed;
}

