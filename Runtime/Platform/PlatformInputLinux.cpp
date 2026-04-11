#include "PlatformInput.h"

#if __linux__

#include "Runtime/Platform/Client.h"
#include "Runtime/CoreGlobals.h"
UHPlatformInput* GPlatformInput;

// mouse button callback
void MouseButtonCallback(GLFWwindow* Window, int32_t Button, int32_t Action, int32_t Mods)
{
	if (GPlatformInput == nullptr)
	{
		return;
	}

	if (Button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (Action == GLFW_PRESS)
		{
			// bIsLeftMousePressed = true
			GPlatformInput->SetLeftMousePressed(true);
		}
		else if (Action == GLFW_RELEASE)
		{
			// bIsLeftMousePressed = false
			GPlatformInput->SetLeftMousePressed(false);
		}
	}

	if (Button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (Action == GLFW_PRESS)
		{
			// bIsRightMousePressed = true
			GPlatformInput->SetRightMousePressed(true);
		}
		else if (Action == GLFW_RELEASE)
		{
			// bIsRightMousePressed = false
			GPlatformInput->SetRightMousePressed(false);
		}
	}
}

// mouse move callback
void MouseMoveCallback(GLFWwindow* Window, double PosX, double PosY)
{
	if (GPlatformInput == nullptr)
	{
		return;
	}

	GPlatformInput->CalculateMouseMovement(static_cast<int32_t>(PosX), static_cast<int32_t>(PosY));
}

// keyboard callback
void KeyboardCallback(GLFWwindow* Window, int32_t Key, int32_t Scancode, int32_t Action, int32_t Mods)
{
	if (GPlatformInput == nullptr)
	{
		return;
	}

	bool bPressed = (Action == GLFW_PRESS);

	// bCurrentKeyState[Key] = bPressed
	if (isalpha((char)Key))
	{
		GPlatformInput->SetKeyPressed((char)Key, bPressed);
	}
	else
	{
		if (Key == GLFW_KEY_LEFT_ALT || Key == GLFW_KEY_RIGHT_ALT)
		{
			GPlatformInput->SetKeyPressed(UH_ENUM_VALUE(UHSystemKey::Alt), bPressed);
		}

		if (Key == GLFW_KEY_LEFT_CONTROL || Key == GLFW_KEY_RIGHT_CONTROL)
		{
			GPlatformInput->SetKeyPressed(UH_ENUM_VALUE(UHSystemKey::Control), bPressed);
		}
		
		if (Key == GLFW_KEY_ENTER)
		{
			GPlatformInput->SetKeyPressed(UH_ENUM_VALUE(UHSystemKey::Enter), bPressed);
		}
	}
}

bool UHPlatformInput::InitInput()
{
	GPlatformInput = this;

	// set input mode for better movement and register input callbacks
	GLFWwindow* Window = (GLFWwindow*)ClientCache->GetNativeWindow();
	
	// select the smoothest mouse input as possible
	if (glfwRawMouseMotionSupported())
	{
		glfwSetInputMode(Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}
	else
	{
		glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	glfwSetMouseButtonCallback(Window, MouseButtonCallback);
	glfwSetCursorPosCallback(Window, MouseMoveCallback);
	glfwSetKeyCallback(Window, KeyboardCallback);

	return true;
}

void UHPlatformInput::ParseInputData(void* InData)
{
	// not needed for Linux
}

void UHPlatformInput::GetMousePosition(long& X, long& Y) const
{
	double MousePosX = 0;
	double MousePosY = 0;

	GLFWwindow* Window = (GLFWwindow*)ClientCache->GetNativeWindow();
	glfwGetCursorPos(Window, &MousePosX, &MousePosY);

	X = static_cast<long>(MousePosX);
	Y = static_cast<long>(MousePosY);
}

#endif