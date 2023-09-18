#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

// Button GUI
class UHButton : public UHGUIControlBase
{
public:
	UHButton();
	UHButton(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
};

#endif