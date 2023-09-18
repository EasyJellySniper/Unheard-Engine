#pragma once
#include "GUIControl.h"

#if WITH_EDITOR

// check box GUI
class UHCheckBox : public UHGUIControlBase
{
public:
	UHCheckBox();
	UHCheckBox(HWND InControl, UHGUIProperty InProperties = UHGUIProperty());
	UHCheckBox(const std::string GUIName, UHGUIProperty InProperties);

	UHCheckBox& Checked(const bool bInFlag);
	bool IsChecked() const;
};

#endif