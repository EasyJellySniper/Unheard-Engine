#include "ListBox.h"

#if WITH_EDITOR
#include <windowsx.h>
#include "Runtime/Classes/Utility.h"

UHListBox::UHListBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHListBox& UHListBox::AddItem(std::wstring InItem)
{
	ListBox_AddString(ControlObj, InItem.c_str());
	return *this;
}

UHListBox& UHListBox::AddItem(std::string InItem)
{
	AddItem(UHUtilities::ToStringW(InItem));
	return *this;
}

UHListBox& UHListBox::Select(int32_t InIndex)
{
	ListBox_SetCurSel(ControlObj, InIndex);
	return *this;
}

int32_t UHListBox::GetSelectedIndex() const
{
	return ListBox_GetCurSel(ControlObj);
}

void UHListBox::Reset()
{
	ListBox_ResetContent(ControlObj);
}

#endif