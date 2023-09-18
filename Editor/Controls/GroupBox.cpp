#include "GroupBox.h"

#if WITH_EDITOR

UHGroupBox::UHGroupBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHGroupBox::UHGroupBox(const std::string GUIName, UHGUIProperty InProperties)
	: UHGUIControlBase()
{
	DWORD Style = WS_CHILD | BS_GROUPBOX | BS_CENTER;
	if (InProperties.bClipped)
	{
		Style |= WS_CLIPCHILDREN;
	}

	ControlObj = CreateWindowA("BUTTON", GUIName.c_str(), Style
		, InProperties.InitPosX, InProperties.InitPosY, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, nullptr, InProperties.Instance, nullptr);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

#endif