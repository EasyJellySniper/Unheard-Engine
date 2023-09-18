#include "FloatDetailGUI.h"

#if WITH_EDITOR
UHFloatDetailGUI::UHFloatDetailGUI(std::string InName, UHGUIProperty InGUIProperty)
	: UHDetailGUIBase(InName)
{
	const SIZE TextSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, InName);
	const SIZE ValueSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, "10000.0f");
	Name = MakeUnique<UHLabel>(InName, InGUIProperty.SetWidth(TextSize.cx).SetHeight(TextSize.cy));

	const int32_t EditControlStartX = InGUIProperty.InitWidth + 20;
	EditControl = MakeUnique<UHTextBox>(InName, InGUIProperty.SetPosX(EditControlStartX).SetWidth(ValueSize.cx));
}

void UHFloatDetailGUI::BindCallback(std::function<void(std::string)> InCallback)
{
	EditControl->OnEditProperty.push_back(InCallback);
}

void UHFloatDetailGUI::SetValue(float InVal)
{
	EditControl->Content(InVal);
}

float UHFloatDetailGUI::GetValue() const
{
	return EditControl->Parse<float>();
}
#endif