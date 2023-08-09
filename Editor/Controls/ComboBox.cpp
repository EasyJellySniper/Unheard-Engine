#include "ComboBox.h"

#if WITH_DEBUG
#include <windowsx.h>
#include <CommCtrl.h>

UHComboBox::UHComboBox()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHComboBox::UHComboBox(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

UHComboBox::UHComboBox(UHGUIProperty InProperties)
{
	ControlObj = CreateWindow(WC_COMBOBOX, L"",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_VSCROLL,
		InProperties.InitPosX, InProperties.InitPosY, InProperties.InitWidth, InProperties.InitHeight, InProperties.ParentWnd, nullptr, InProperties.Instance, nullptr);

	InternalInit(ControlObj, InProperties);
	bIsManuallyCreated = true;
}

UHComboBox& UHComboBox::Init(std::wstring DefaultValue, const std::vector<std::wstring>& Options, int32_t MinVisible)
{
	ComboBox_ResetContent(ControlObj);
	for (const std::wstring& Option : Options)
	{
		ComboBox_AddString(ControlObj, Option.c_str());
	}
	ComboBox_SelectString(ControlObj, 0, DefaultValue.c_str());

	if (MinVisible > 0)
	{
		ComboBox_SetMinVisible(ControlObj, MinVisible);
	}

	return *this;
}

UHComboBox& UHComboBox::Select(std::wstring InItem)
{
	ComboBox_SelectString(ControlObj, 0, InItem.c_str());
	return *this;
}

int32_t UHComboBox::GetSelectedIndex() const
{
	return ComboBox_GetCurSel(ControlObj);
}

int32_t UHComboBox::GetItemCount() const
{
	return ComboBox_GetCount(ControlObj);
}

std::wstring UHComboBox::GetSelectedItem() const
{
	wchar_t Buff[1024];
	memset(Buff, 0, 1024);
	Buff[1023] = L'\0';
	ComboBox_GetText(ControlObj, Buff, 1024);

	return Buff;
}

#endif