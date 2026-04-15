#pragma once
#include <cstdint>
// cross-platform input header

enum class UHSystemKey : uint32_t
{
	Enter = 0x0D,
	Control = 0x11,
	Alt = 0x12
};

class UHClient;
class UHPlatformInput
{
public:
	UHPlatformInput()
		: UHPlatformInput(nullptr)
	{

	}
	UHPlatformInput(UHClient* InClient);

	bool InitInput();
	void SetInputEnabled(bool bInFlag);
	void ParseInputData(void* InData);
	void ResetMouseData();
	void ResetMouseState();

	// set key state manually, can be useful for editor inputs
	void SetLeftMousePressed(bool bFlag);
	void SetRightMousePressed(bool bFlag);
	void SetKeyPressed(char CharKey, bool bFlag);
	void SetKeyPressed(int32_t VkKey, bool bFlag);
	void CalculateMouseMovement(const double CurrX, const double CurrY);

	// mouse delta calculated based on cached cursor position, might not be as smooth as the movement function
	void GetMouseDelta(double& X, double& Y) const;
	// mouse movement returned by input api directly
	void GetMouseMovement(double& X, double& Y) const;
	// mouse position
	void GetMousePosition(double& X, double& Y) const;

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
	// key state, true for keydown false for keyup
	// the keypoint is to decide key up/hold/down event with states
	// the states are stored Virtual-Key based codes, so bCurrentKeyState[65] = true means "A" is pressed.
	static constexpr int32_t MaxKeyStates = 256;
	bool bPreviousKeyState[MaxKeyStates];
	bool bCurrentKeyState[MaxKeyStates];

	bool bIsLeftMousePressed;
	bool bIsRightMousePressed;
	bool bPreviousLeftMousePressed;
	bool bPreviousRightMousePressed;
	bool bEnableInput;

	double LastMousePosX;
	double LastMousePosY;
	double LastMouseMovementX;
	double LastMouseMovementY;

	UHClient* ClientCache;
};