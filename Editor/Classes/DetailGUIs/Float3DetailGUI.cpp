#include "Float3DetailGUI.h"

#if WITH_EDITOR

UHFloat3DetailGUI::UHFloat3DetailGUI(std::string InName, UHGUIProperty InGUIProperty)
	: UHDetailGUIBase(InName)
{
	const SIZE TextSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, InName);
	const SIZE ValueSize = UHEditorUtil::GetTextSize(InGUIProperty.ParentWnd, "10000.0f");
	Name = MakeUnique<UHLabel>(InName, InGUIProperty.SetWidth(TextSize.cx).SetHeight(TextSize.cy));

	const int32_t EditControlStartX = InGUIProperty.InitWidth + 20;
	for (int32_t Idx = 0; Idx < 3; Idx++)
	{
		EditControl[Idx] = MakeUnique<UHTextBox>(InName, InGUIProperty.SetPosX(EditControlStartX + Idx * (ValueSize.cx + 5)).SetWidth(ValueSize.cx));
	}
}

void UHFloat3DetailGUI::BindCallback(std::function<void(std::string)> InCallback)
{
	for (int32_t Idx = 0; Idx < 3; Idx++)
	{
		EditControl[Idx]->OnEditProperty.push_back(InCallback);
	}
}

void UHFloat3DetailGUI::SetValue(XMFLOAT3 InVal)
{
	EditControl[0]->Content(InVal.x);
	EditControl[1]->Content(InVal.y);
	EditControl[2]->Content(InVal.z);
}

XMFLOAT3 UHFloat3DetailGUI::GetValue() const
{
	XMFLOAT3 NewValue{};
	NewValue.x = EditControl[0]->Parse<float>();
	NewValue.y = EditControl[1]->Parse<float>();
	NewValue.z = EditControl[2]->Parse<float>();
	return NewValue;
}

#endif