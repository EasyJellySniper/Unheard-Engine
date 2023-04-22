#include "ParameterNodeGUI.h"

#if WITH_DEBUG
#include "EditorUtils.h"
#include "../../Runtime/Classes/Utility.h"

UHFloatNodeGUI::UHFloatNodeGUI()
	: Node(nullptr)
{

}

UHFloat2NodeGUI::UHFloat2NodeGUI()
	: Node(nullptr)
{

}

UHFloat3NodeGUI::UHFloat3NodeGUI()
	: Node(nullptr)
{

}

UHFloat4NodeGUI::UHFloat4NodeGUI()
	: Node(nullptr)
{

}

void UHFloatNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetValue(std::stof(UHEditorUtil::GetEditControlText(InputsTextField[0])));
}

void UHFloat2NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT2 Value;
	Value.x = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[0]));
	Value.y = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[1]));
	Node->SetValue(Value);
}

void UHFloat3NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT3 Value;
	Value.x = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[0]));
	Value.y = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[1]));
	Value.z = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[2]));
	Node->SetValue(Value);
}

void UHFloat4NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT4 Value;
	Value.x = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[0]));
	Value.y = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[1]));
	Value.z = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[2]));
	Value.w = std::stof(UHEditorUtil::GetEditControlText(InputsTextField[3]));
	Node->SetValue(Value);
}

void UHFloatNodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloatNode*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat2NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat2Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat3NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat3Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloat4NodeGUI::PreAddingPins()
{
	Node = static_cast<UHFloat4Node*>(GraphNode);
	InputTextFieldLength = 35;
}

void UHFloatNodeGUI::PostAddingPins()
{
	// sync value to control
	UHEditorUtil::SetEditControl(InputsTextField[0], UHUtilities::FloatToWString(Node->GetValue()));
}

void UHFloat2NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT2 Value = Node->GetValue();
	UHEditorUtil::SetEditControl(InputsTextField[0], UHUtilities::FloatToWString(Value.x));
	UHEditorUtil::SetEditControl(InputsTextField[1], UHUtilities::FloatToWString(Value.y));
}

void UHFloat3NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT3 Value = Node->GetValue();
	UHEditorUtil::SetEditControl(InputsTextField[0], UHUtilities::FloatToWString(Value.x));
	UHEditorUtil::SetEditControl(InputsTextField[1], UHUtilities::FloatToWString(Value.y));
	UHEditorUtil::SetEditControl(InputsTextField[2], UHUtilities::FloatToWString(Value.z));
}

void UHFloat4NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT4 Value = Node->GetValue();
	UHEditorUtil::SetEditControl(InputsTextField[0], UHUtilities::FloatToWString(Value.x));
	UHEditorUtil::SetEditControl(InputsTextField[1], UHUtilities::FloatToWString(Value.y));
	UHEditorUtil::SetEditControl(InputsTextField[2], UHUtilities::FloatToWString(Value.z));
	UHEditorUtil::SetEditControl(InputsTextField[3], UHUtilities::FloatToWString(Value.w));
}

#endif