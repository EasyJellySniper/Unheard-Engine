#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

class UHGroupBox : public UHGUIControlBase
{
public:
	UHGroupBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
	UHGroupBox(const std::string GUIName, UHGUIProperty InProperties);
};

#endif