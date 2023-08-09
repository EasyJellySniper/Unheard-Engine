#pragma once
#include "GUIControl.h"

#if WITH_DEBUG

// radio button control
class UHRadioButton : public UHGUIControlBase
{
public:
	UHRadioButton(HWND InControl, UHGUIProperty InProperties);
	UHRadioButton(std::string InGUIName, UHGUIProperty InProperties, bool bIsLeftText);

	UHRadioButton& Checked(bool bInFlag);
	bool bIsChecked() const;
};

#endif