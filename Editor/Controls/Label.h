#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

// label control
class UHLabel : public UHGUIControlBase
{
public:
	UHLabel();
	UHLabel(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
	UHLabel(std::string InName, UHGUIProperty InProperties = UHGUIProperty());
};

#endif