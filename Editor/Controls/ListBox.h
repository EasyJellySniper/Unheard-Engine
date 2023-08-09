#pragma once
#include "GUIControl.h"

#if WITH_DEBUG

class UHListBox : public UHGUIControlBase
{
public:
	UHListBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());

	UHListBox& AddItem(std::wstring InItem);
	UHListBox& AddItem(std::string InItem);

	UHListBox& Select(int32_t InIndex);
	int32_t GetSelectedIndex() const;
};

#endif