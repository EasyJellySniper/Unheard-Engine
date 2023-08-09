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
	Node->SetValue(InputsTextFields[0]->Parse<float>());
}

void UHFloat2NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT2 Value;
	Value.x = InputsTextFields[0]->Parse<float>();
	Value.y = InputsTextFields[1]->Parse<float>();
	Node->SetValue(Value);
}

void UHFloat3NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT3 Value;
	Value.x = InputsTextFields[0]->Parse<float>();
	Value.y = InputsTextFields[1]->Parse<float>();
	Value.z = InputsTextFields[2]->Parse<float>();
	Node->SetValue(Value);
}

void UHFloat4NodeGUI::SetDefaultValueFromGUI()
{
	XMFLOAT4 Value;
	Value.x = InputsTextFields[0]->Parse<float>();
	Value.y = InputsTextFields[1]->Parse<float>();
	Value.z = InputsTextFields[2]->Parse<float>();
	Value.w = InputsTextFields[3]->Parse<float>();
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
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Node->GetValue()));
}

void UHFloat2NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT2 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.x));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.y));
}

void UHFloat3NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT3 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.x));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.y));
	InputsTextFields[2]->SetText(UHUtilities::FloatToWString(Value.z));
}

void UHFloat4NodeGUI::PostAddingPins()
{
	// sync value to control
	XMFLOAT4 Value = Node->GetValue();
	InputsTextFields[0]->SetText(UHUtilities::FloatToWString(Value.x));
	InputsTextFields[1]->SetText(UHUtilities::FloatToWString(Value.y));
	InputsTextFields[2]->SetText(UHUtilities::FloatToWString(Value.z));
	InputsTextFields[3]->SetText(UHUtilities::FloatToWString(Value.w));
}

#endif