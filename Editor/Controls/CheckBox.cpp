#include "CheckBox.h"

#if WITH_EDITOR
#include <windowsx.h>

UHCheckBox::UHCheckBox()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHCheckBox::UHCheckBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHCheckBox::UHCheckBox(const std::string GUIName, UHGUIProperty InProperties)
{
	ControlObj = CreateWindowA("BUTTON", GUIName.c_str(), WS_CHILD | BS_AUTOCHECKBOX
		, InProperties.InitPosX, InProperties.InitPosY, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, nullptr, InProperties.Instance, nullptr);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

UHCheckBox& UHCheckBox::Checked(const bool bInFlag)
{
	Button_SetCheck(ControlObj, (bInFlag) ? BST_CHECKED : BST_UNCHECKED);
	return *this;
}

bool UHCheckBox::IsChecked() const
{
	int32_t IsChecked = Button_GetCheck(ControlObj);
	return (IsChecked == BST_CHECKED);
}

#endif