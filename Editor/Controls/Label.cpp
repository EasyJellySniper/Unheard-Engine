#include "Label.h"

#if WITH_DEBUG

UHLabel::UHLabel()
	: UHGUIControlBase(nullptr, UHGUIProperty())
{

}

UHLabel::UHLabel(HWND InControl, UHGUIProperty InProperties)
	: UHGUIControlBase(InControl, InProperties)
{

}

#endif