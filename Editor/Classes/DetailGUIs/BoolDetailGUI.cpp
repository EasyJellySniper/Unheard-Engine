#include "BoolDetailGUI.h"

#if WITH_EDITOR
UHBoolDetailGUI::UHBoolDetailGUI(std::string InName, UHGUIProperty InGUIProperty)
	: UHDetailGUIBase(InName)
{
	const SIZE TextSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, InName);
	Name = MakeUnique<UHLabel>(InName, InGUIProperty.SetWidth(TextSize.cx).SetHeight(TextSize.cy));

	const int32_t CheckBoxControlStartX = InGUIProperty.InitWidth + 20;
	CheckBoxControl = MakeUnique<UHCheckBox>("", InGUIProperty.SetPosX(CheckBoxControlStartX).SetWidth(TextSize.cx));
}

void UHBoolDetailGUI::BindCallback(std::function<void(std::string)> InCallback)
{
	CheckBoxControl->OnEditProperty.push_back(InCallback);
}

void UHBoolDetailGUI::SetValue(bool InVal)
{
	CheckBoxControl->Checked(InVal);
}

bool UHBoolDetailGUI::GetValue() const
{
	return CheckBoxControl->IsChecked();
}
#endif