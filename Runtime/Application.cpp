#include "Application.h"
#include "Platform/Platform.h"

int UHApplication::Run()
{
	return UHPlatform::PlatformRun();
}