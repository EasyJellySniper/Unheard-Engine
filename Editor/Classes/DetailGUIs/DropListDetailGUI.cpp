#include "DropListDetailGUI.h"

#if WITH_EDITOR

UHDropListDetailGUI::UHDropListDetailGUI(std::string InName, UHGUIProperty InGUIProperty)
	: UHDetailGUIBase(InName)
{
	const SIZE TextSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, InName);
	Name = MakeUnique<UHLabel>(InName, InGUIProperty.SetWidth(TextSize.cx).SetHeight(TextSize.cy));

	RECT R;
	GetWindowRect(InGUIProperty.ParentWnd, &R);

	const int32_t ControlStartX = InGUIProperty.InitWidth + 20;
	const int32_t ControlWidth = R.right - R.left - ControlStartX - 10;
	ComboBoxControl = MakeUnique<UHComboBox>(InGUIProperty.SetPosX(ControlStartX).SetWidth(ControlWidth).SetHeight(500));
}

void UHDropListDetailGUI::BindCallback(std::function<void(std::string)> InCallback)
{
	ComboBoxControl->OnEditProperty.push_back(InCallback);
}

void UHDropListDetailGUI::InitList(std::vector<std::wstring> InItems)
{
	ComboBoxControl->Init(L"", InItems);
}

void UHDropListDetailGUI::SetValue(std::wstring InVal)
{
	ComboBoxControl->Select(InVal);
}

std::wstring UHDropListDetailGUI::GetValue() const
{
	return ComboBoxControl->GetSelectedItem();
}

#endif