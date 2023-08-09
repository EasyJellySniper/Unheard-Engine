#pragma once
#include "GUIControl.h"

#if WITH_DEBUG

// check box GUI
class UHCheckBox : public UHGUIControlBase
{
public:
	UHCheckBox();
	UHCheckBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());

	UHCheckBox& Checked(const bool bInFlag);
	bool IsChecked() const;
};

#endif