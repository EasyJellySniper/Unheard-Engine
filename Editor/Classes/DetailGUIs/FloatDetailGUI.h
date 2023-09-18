#pragma once
#include "DetailGUIs.h"

#if WITH_EDITOR

class UHFloatDetailGUI : public UHDetailGUIBase
{
public:
	UHFloatDetailGUI(std::string InName, UHGUIProperty InGUIProperty);
	void BindCallback(std::function<void(std::string)> InCallback) override;

	void SetValue(float InVal);
	float GetValue() const;

private:
	UniquePtr<UHLabel> Name;
	UniquePtr<UHTextBox> EditControl;
};

#endif