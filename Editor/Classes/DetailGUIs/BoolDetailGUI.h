#pragma once
#include "DetailGUIs.h"

#if WITH_EDITOR

class UHBoolDetailGUI : public UHDetailGUIBase
{
public:
	UHBoolDetailGUI(std::string InName, UHGUIProperty InGUIProperty);
	void BindCallback(std::function<void(std::string)> InCallback) override;

	void SetValue(bool InVal);
	bool GetValue() const;

private:
	UniquePtr<UHLabel> Name;
	UniquePtr<UHCheckBox> CheckBoxControl;
};

#endif