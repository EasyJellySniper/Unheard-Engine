#include "Platform.h"

// Linux platform
#ifdef __linux__

#include <GLFW/glfw3.h>
bool UHPlatform::Initialize(UHApplication* InApp)
{
	glfwInit();

	// setup hints
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* Window = glfwCreateWindow(800, 600, ENGINE_NAME, nullptr, nullptr);

	while (!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
	}

	Client = MakeUnique<UHClient>();
	Client->SetClientWindow(Window);

	return true;
}

void UHPlatform::Shutdown()
{
	GLFWwindow* Window = (GLFWwindow*)Client->GetNativeWindow();
	glfwDestroyWindow(Window);
	glfwTerminate();
}

#endif