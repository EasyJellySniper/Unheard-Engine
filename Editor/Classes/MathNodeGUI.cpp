#include "MathNodeGUI.h"

#if WITH_DEBUG
#include "EditorUtils.h"

UHMathNodeGUI::UHMathNodeGUI()
	: Node(nullptr)
{

}

void UHMathNodeGUI::SetDefaultValueFromGUI()
{
	Node->SetOperator(static_cast<UHMathNodeOperator>(UHEditorUtil::GetComboBoxSelectedIndex(ComboBox)));
}

void UHMathNodeGUI::PreAddingPins()
{
	Node = static_cast<UHMathNode*>(GraphNode);

	std::string OperatorString[] = { "+","-","*","/" };
	for (const std::string& Operator : OperatorString)
	{
		ComboBoxItems.push_back(Operator);
	}

	ComboBoxMeasure.PosX = 50;
	ComboBoxMeasure.PosY = 20;
	ComboBoxMeasure.Width = 50;
	ComboBoxMeasure.Height = 100;
	ComboBoxMeasure.Margin = 25;
}

void UHMathNodeGUI::PostAddingPins()
{
	std::string OperatorString[] = { "+","-","*","/" };
	int32_t Operator = Node->GetOperator();
	UHEditorUtil::SelectComboBox(ComboBox, OperatorString[Operator]);
}

#endif