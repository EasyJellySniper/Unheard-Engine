#include "Button.h"

#if WITH_EDITOR

UHButton::UHButton()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHButton::UHButton(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

#endif