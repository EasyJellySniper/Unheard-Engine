#include "Client.h"

#if __linux__

void UHClient::SetWindowSize(const int32_t Width, const int32_t Height)
{
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	glfwSetWindowSize(Window, Width, Height);
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
	GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
	int32_t MonitorX, MonitorY;
	glfwGetMonitorPos(Monitor, &MonitorX, &MonitorY);
	
	if (bIsFullscreen)
	{
		// going fullscreen
		const GLFWvidmode* Mode = glfwGetVideoMode(Monitor);

		glfwSetWindowMonitor(
			Window,
			Monitor,
			0, 0,
			Mode->width,
			Mode->height,
			Mode->refreshRate
		);

		glfwSetWindowAttrib(Window, GLFW_DECORATED, GLFW_FALSE);
		glfwShowWindow(Window);
		glfwFocusWindow(Window);
		glfwRequestWindowAttention(Window);
	}
	else
	{
		// going window
		glfwSetWindowMonitor(
			Window,
			nullptr,
			MonitorX + 10, MonitorY + 10,
			Width, Height,
			0
		);

		glfwSetWindowAttrib(Window, GLFW_RESIZABLE, GLFW_TRUE);
		glfwSetWindowAttrib(Window, GLFW_DECORATED, GLFW_TRUE);
		glfwShowWindow(Window);
		glfwFocusWindow(Window);
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