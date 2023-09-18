#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

// text box control
class UHTextBox : public UHGUIControlBase
{
public:
	UHTextBox();
	UHTextBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
	UHTextBox(std::string InGUIName, UHGUIProperty InProperties);

	template <typename T>
	UHTextBox& Content(T InValue);

	template <typename T>
	T Parse();
};

#endif