// shared implementaion for the platform inputs
// certain functions can be reused
#include "PlatformInput.h"
#include <cctype>
#include <memory>
#include "Runtime/CoreGlobals.h"

UHPlatformInput::UHPlatformInput(UHClient* InClient)
	: bPreviousKeyState{}
	, bCurrentKeyState{}
	, bIsLeftMousePressed(false)
	, bIsRightMousePressed(false)
	, bPreviousLeftMousePressed(false)
	, bPreviousRightMousePressed(false)
	, bEnableInput(true)
	, LastMousePosX(0)
	, LastMousePosY(0)
	, LastMouseMovementX(0)
	, LastMouseMovementY(0)
	, ClientCache(InClient)
{

}

void UHPlatformInput::SetInputEnabled(bool bInFlag)
{
	bEnableInput = bInFlag;
}

void UHPlatformInput::ResetMouseData()
{
	LastMouseMovementX = 0;
	LastMouseMovementY = 0;
	LastMousePosX = 0;
	LastMousePosY = 0;
}

void UHPlatformInput::ResetMouseState()
{
	bIsLeftMousePressed = false;
	bPreviousLeftMousePressed = false;
	bIsRightMousePressed = false;
	bPreviousRightMousePressed = false;
}

void UHPlatformInput::SetLeftMousePressed(bool bFlag)
{
	bIsLeftMousePressed = bFlag;
}

void UHPlatformInput::SetRightMousePressed(bool bFlag)
{
	bIsRightMousePressed = bFlag;
}

void UHPlatformInput::SetKeyPressed(char CharKey, bool bFlag)
{
	// set key pressed (alphabetic version)
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
		bCurrentKeyState[(int32_t)CharKey] = bFlag;
	}
}

void UHPlatformInput::SetKeyPressed(int32_t VkKey, bool bFlag)
{
	// set key pressed (Vk Key version)
	bCurrentKeyState[VkKey] = bFlag;
}

void UHPlatformInput::CalculateMouseMovement(const int32_t CurrX, const int32_t CurrY)
{
	LastMouseMovementX = CurrX - LastMousePosX;
	LastMouseMovementY = CurrY - LastMousePosY;
}

void UHPlatformInput::GetMouseMovement(int32_t& X, int32_t& Y) const
{
	X = LastMouseMovementX;
	Y = LastMouseMovementY;
}

void UHPlatformInput::GetMouseDelta(int32_t& X, int32_t& Y) const
{
	long MousePosX, MousePosY;
	GetMousePosition(MousePosX, MousePosY);

	X = MousePosX - LastMousePosX;
	Y = MousePosY - LastMousePosY;
}

void UHPlatformInput::CacheKeyStates()
{
	// cache key states of current frame, this should be called at the end of update functions, or at least before any input checking
	UHMEMCOPY(bPreviousKeyState, bCurrentKeyState, sizeof(bool) * MaxKeyStates);
	bPreviousLeftMousePressed = bIsLeftMousePressed;
	bPreviousRightMousePressed = bIsRightMousePressed;

	// cache last mouse pos too
	GetMousePosition(LastMousePosX, LastMousePosY);
}

bool UHPlatformInput::IsKeyHold(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyHold((int32_t)CharKey);
}

bool UHPlatformInput::IsKeyDown(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyDown((int32_t)CharKey);
}

bool UHPlatformInput::IsKeyUp(char CharKey) const
{
	if (isalpha(CharKey))
	{
		CharKey = toupper(CharKey);
	}
	return IsKeyUp((int32_t)CharKey);
}

bool UHPlatformInput::IsKeyHold(int32_t VkKey) const
{
	return bCurrentKeyState[VkKey] && bPreviousKeyState[VkKey] && bEnableInput;
}

bool UHPlatformInput::IsKeyDown(int32_t VkKey) const
{
	return bCurrentKeyState[VkKey] && !bPreviousKeyState[VkKey] && bEnableInput;
}

bool UHPlatformInput::IsKeyUp(int32_t VkKey) const
{
	return !bCurrentKeyState[VkKey] && bPreviousKeyState[VkKey] && bEnableInput;
}

bool UHPlatformInput::IsLeftMouseHold() const
{
	return bIsLeftMousePressed && bPreviousLeftMousePressed && bEnableInput;
}

bool UHPlatformInput::IsRightMouseHold() const
{
	return bIsRightMousePressed && bPreviousRightMousePressed && bEnableInput;
}

bool UHPlatformInput::IsLeftMouseDown() const
{
	return bIsLeftMousePressed && !bPreviousLeftMousePressed && bEnableInput;
}

bool UHPlatformInput::IsRightMouseDown() const
{
	return bIsRightMousePressed && !bPreviousRightMousePressed && bEnableInput;
}

bool UHPlatformInput::IsLeftMouseUp() const
{
	return !bIsLeftMousePressed && bPreviousLeftMousePressed && bEnableInput;
}

bool UHPlatformInput::IsRightMouseUp() const
{
	return !bIsRightMousePressed && bPreviousRightMousePressed && bEnableInput;
}