#include "CheckBox.h"

#if WITH_DEBUG
#include <windowsx.h>

UHCheckBox::UHCheckBox()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHCheckBox::UHCheckBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

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