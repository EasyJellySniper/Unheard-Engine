#pragma once
#include "DetailGUIs.h"

#if WITH_EDITOR

class UHFloat3DetailGUI : public UHDetailGUIBase
{
public:
	UHFloat3DetailGUI(std::string InName, UHGUIProperty InGUIProperty);
	void BindCallback(std::function<void(std::string)> InCallback) override;

	void SetValue(XMFLOAT3 InVal);
	XMFLOAT3 GetValue() const;

private:
	UniquePtr<UHLabel> Name;
	UniquePtr<UHTextBox> EditControl[3];
};

#endif