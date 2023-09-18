#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

// combo box control
class UHComboBox : public UHGUIControlBase
{
public:
	UHComboBox();
	UHComboBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
	UHComboBox(UHGUIProperty InProperties);

	UHComboBox& Init(std::wstring DefaultValue, const std::vector<std::wstring>& Options, int32_t MinVisible = 0);
	UHComboBox& Select(std::wstring InItem);

	int32_t GetSelectedIndex() const;
	int32_t GetItemCount() const;
	std::wstring GetSelectedItem() const;
};

#endif