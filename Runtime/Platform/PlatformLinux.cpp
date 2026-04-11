#include "Platform.h"

// Linux platform
#ifdef __linux__

bool UHPlatform::Initialize(UHApplication* InApp)
{
	glfwInit();

	// setup hints
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	// create window but hide it at the beginning, it will be displayed with configs later by engine
	GLFWwindow* Window = glfwCreateWindow(800, 600, ENGINE_NAME, nullptr, nullptr);

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