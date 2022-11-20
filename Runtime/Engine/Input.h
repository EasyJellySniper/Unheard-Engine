#pragma once
#include "../../framework.h"
#include <cstdint>

// input device type enum
enum InputDeviceType
{
	Mouse, Keyboard
};

// raw input class used in Unheard Engine, which is mirrored from my previous works
class UHRawInput
{
public:
	UHRawInput();

	bool InitRawInput();
	void ParseInputData(LPARAM LParam);
	void ResetMouseData();
	void ResetMouseState();
	RAWMOUSE GetMouseData() const;

	void CacheKeyStates();
	bool IsKeyHold(char CharKey) const;
	bool IsKeyDown(char CharKey) const;
	bool IsKeyUp(char CharKey) const;
	bool IsKeyHold(int32_t VkKey) const;
	bool IsKeyDown(int32_t VkKey) const;
	bool IsKeyUp(int32_t VkKey) const;

	bool IsLeftMouseHold() const;
	bool IsRightMouseHold() const;
	bool IsLeftMouseDown() const;
	bool IsRightMouseDown() const;
	bool IsLeftMouseUp() const;
	bool IsRightMouseUp() const;

private:
	RAWINPUTDEVICE RawInputDevice[2];
	RAWINPUT RawInputData;

	// key state, true for keydown false for keyup
	// the keypoint is to decide key up/hold/down event with states
	bool bPreviousKeyState[256];
	bool bCurrentKeyState[256];

	bool bIsLeftMousePressed;
	bool bIsRightMousePressed;
	bool bPreviousLeftMousePressed;
	bool bPreviousRightMousePressed;
};