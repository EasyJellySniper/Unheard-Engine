#include "GroupBox.h"

#if WITH_DEBUG

UHGroupBox::UHGroupBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHGroupBox::UHGroupBox(const std::string GUIName, UHGUIProperty InProperties)
	: UHGUIControlBase()
{
	ControlObj = CreateWindowA("BUTTON", GUIName.c_str(), WS_CHILD | BS_GROUPBOX | BS_CENTER
		, InProperties.InitPosX, InProperties.InitPosY, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, nullptr, InProperties.Instance, nullptr);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

#endif