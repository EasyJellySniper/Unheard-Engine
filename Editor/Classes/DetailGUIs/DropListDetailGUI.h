#pragma once
#include "DetailGUIs.h"

#if WITH_EDITOR

class UHDropListDetailGUI : public UHDetailGUIBase
{
public:
	UHDropListDetailGUI(std::string InName, UHGUIProperty InGUIProperty);
	void BindCallback(std::function<void(std::string)> InCallback) override;

	void InitList(std::vector<std::wstring> InItems);
	void SetValue(std::wstring InVal);
	std::wstring GetValue() const;

private:
	UniquePtr<UHLabel> Name;
	UniquePtr<UHComboBox> ComboBoxControl;
};

#endif