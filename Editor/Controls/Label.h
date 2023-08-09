#pragma once
#include "GUIControl.h"

#if WITH_DEBUG

// label control
class UHLabel : public UHGUIControlBase
{
public:
	UHLabel();
	UHLabel(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
};

#endif