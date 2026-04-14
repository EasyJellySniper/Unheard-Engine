#include "Client.h"

#if __linux__

void UHClient::SetWindowPosition(const int32_t Width, const int32_t Height)
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	glfwSetWindowPos(Window, Width, Height);
}

void UHClient::GetWindowSize(int32_t& OutWidth, int32_t& OutHeight) const
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	int TempWidth, TempHeight;
	glfwGetWindowSize(Window, &TempWidth, &TempHeight);
	OutWidth = TempWidth;
	OutHeight = TempHeight;
}

void UHClient::SetWindowStyle(const bool bIsFullscreen, const int32_t Width, const int32_t Height)
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;

	if (bIsFullscreen)
	{
		// going fullscreen
		GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* Mode = glfwGetVideoMode(Monitor);

		glfwSetWindowAttrib(Window, GLFW_DECORATED, GLFW_FALSE);
		glfwSetWindowMonitor(
			Window,
			Monitor,
			0, 0,
			Mode->width,
			Mode->height,
			Mode->refreshRate
		);
		glfwShowWindow(Window);
	}
	else
	{
		// going window
		glfwSetWindowAttrib(Window, GLFW_RESIZABLE, GLFW_TRUE);
		glfwSetWindowMonitor(
			Window,
			nullptr,
			0, 0,
			Width, Height,
			0
		);
		glfwShowWindow(Window);
	}
}

void UHClient::SetWindowCaption(const std::string InCaption)
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	glfwSetWindowTitle(Window, InCaption.c_str());
}

bool UHClient::IsWindowMinimized()
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	return glfwGetWindowAttrib(Window, GLFW_ICONIFIED);
}

bool UHClient::IsQuit()
{
	return bIsQuit;
}

void UHClient::ProcessEvents()
{
	// process all events at once
	glfwPollEvents();

	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	bIsQuit = glfwWindowShouldClose(Window);
}

int32_t UHClient::GetDisplayFrequency() const
{
	GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* Mode = glfwGetVideoMode(Monitor);

	return Mode->refreshRate;
}

#endif