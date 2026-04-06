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
	glfwGetWindowSize(Window, OutWidth, OutHeight);
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
	GLFWwindow* Window = (GLFWwindow*)NativeWindow;
	return glfwWindowShouldClose(Window);
}

bool UHClient::ProcessEvents()
{
	glfwPollEvents();
}

#endif