#include "RadioButton.h"

#if WITH_DEBUG
#include <windowsx.h>

UHRadioButton::UHRadioButton(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHRadioButton::UHRadioButton(std::string InGUIName, UHGUIProperty InProperties, bool bIsLeftText)
{
	DWORD Style = BS_RADIOBUTTON | WS_CHILD;
	if (bIsLeftText)
	{
		Style |= BS_LEFTTEXT;
	}

	ControlObj = CreateWindowA("BUTTON", InGUIName.c_str()
		, Style
		, InProperties.InitPosX, InProperties.InitPosY
		, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, NULL, InProperties.Instance, NULL);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

UHRadioButton& UHRadioButton::Checked(bool bInFlag)
{
	Button_SetCheck(ControlObj, (bInFlag) ? BST_CHECKED : BST_UNCHECKED);
	return *this;
}

bool UHRadioButton::bIsChecked() const
{
	int32_t IsChecked = Button_GetCheck(ControlObj);
	return (IsChecked == BST_CHECKED);
}

#endif