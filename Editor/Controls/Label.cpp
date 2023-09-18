#include "Label.h"

#if WITH_EDITOR

UHLabel::UHLabel()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHLabel::UHLabel(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHLabel::UHLabel(std::string InName, UHGUIProperty InProperties)
{
	DWORD Style = WS_CHILD;
	ControlObj = CreateWindowA("STATIC", InName.c_str(), Style
		, InProperties.InitPosX, InProperties.InitPosY, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, nullptr, InProperties.Instance, nullptr);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

#endif