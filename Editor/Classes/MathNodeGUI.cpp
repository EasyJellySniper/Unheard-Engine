#include "MathNodeGUI.h"

#if WITH_EDITOR
#include "EditorUtils.h"
std::wstring GOperatorString[] = { L"+",L"-",L"*",L"/" };

UHMathNodeGUI::UHMathNodeGUI()
	: Node(nullptr)
{

}

void UHMathNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetOperator(static_cast<UHMathNodeOperator>(ComboBox->GetSelectedIndex()));
}

void UHMathNodeGUI::PreAddingPins()
{
	Node = static_cast<UHMathNode*>(GraphNode);
	for (const std::wstring& Operator : GOperatorString)
	{
		ComboBoxItems.push_back(Operator);
	}

	ComboBoxProp.SetPosX(50).SetPosY(20).SetWidth(50).SetHeight(100).SetMarginX(25).SetMinVisibleItem(15);
}

void UHMathNodeGUI::PostAddingPins()
{
	const int32_t Operator = Node->GetOperator();
	ComboBox->Select(GOperatorString[Operator]);
}

#endif