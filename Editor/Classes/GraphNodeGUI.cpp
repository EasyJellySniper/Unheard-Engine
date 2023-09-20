#include "GraphNodeGUI.h"

#if WITH_EDITOR
#include "../../Runtime/Classes/Utility.h"
#include "EditorUtils.h"
#include <algorithm>
#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

UniquePtr<UHPinSelectInfo> GPinSelectInfo;

UHGraphNodeGUI::UHGraphNodeGUI()
	: NodeGUI(nullptr)
	, GUIName("")
	, GraphNode(nullptr)
	, InputTextFieldLength(0)
	, ComboBox(nullptr)
{

}

UHGraphNodeGUI::~UHGraphNodeGUI()
{
	Release();
}

// Output Pin Proc callback
INT_PTR CALLBACK NodeOutputPinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	POINT P;
	GetCursorPos(&P);

	switch (message)
	{
	case WM_LBUTTONDOWN:
		GPinSelectInfo->MouseDownPos = P;
		GPinSelectInfo->CurrOutputPin = reinterpret_cast<UHGraphPin*>(dwRefData);
		break;

	case WM_LBUTTONUP:
		GPinSelectInfo->MouseUpPos = P;
		GPinSelectInfo->bReadyForConnect = true;
		break;

	case WM_RBUTTONUP:
		GPinSelectInfo->RightClickedPin = reinterpret_cast<UHGraphPin*>(dwRefData);
		break;
	}

	return DefSubclassProc(hWnd, message, wParam, lParam);
}

// Input Pin Proc callback
INT_PTR CALLBACK NodeInputPinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	POINT P;
	GetCursorPos(&P);

	switch (message)
	{
	case WM_RBUTTONUP:
		GPinSelectInfo->RightClickedPin = reinterpret_cast<UHGraphPin*>(dwRefData);
		break;
	}

	return DefSubclassProc(hWnd, message, wParam, lParam);
}

// node combo box callback
INT_PTR CALLBACK NodeComboBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (HIWORD(wParam) == CBN_SELCHANGE)
	{
		reinterpret_cast<UHGraphNodeGUI*>(dwRefData)->OnSelectCombobox();
	}

	return DefSubclassProc(hWnd, message, wParam, lParam);
}

void UHGraphNodeGUI::Init(HINSTANCE InInstance, HWND InParent, UHGraphNode* InNode, std::string InGUIName
	, int32_t PosX, int32_t PosY)
{
	Release();
	GraphNode = InNode;
	GraphNode->SetGUI(this);
	GUIName = InGUIName;

	// width & height of node will be measured below
	NodeGUI = MakeUnique<UHGroupBox>(GUIName, UHGUIProperty()
		.SetInstance(InInstance)
		.SetParent(InParent)
		.SetPosX(PosX).SetPosY(PosY)
		.SetWidth(10).SetHeight(10));

	// call back before adding pins
	this->PreAddingPins();

	std::vector<UniquePtr<UHGraphPin>>& Inputs = InNode->GetInputs();
	std::vector<UniquePtr<UHGraphPin>>& Outputs = InNode->GetOutputs();

	HDC NodeDC = GetDC(NodeGUI->GetHwnd());

	int32_t MaxWidth = 0;
	int32_t MaxHeight = 0;
	int32_t WidthMargin = 0;

	// add combobox
	{
		if (ComboBoxItems.size() > 0)
		{
			ComboBox = MakeUnique<UHComboBox>(ComboBoxProp.SetInstance(InInstance).SetParent(NodeGUI->GetHwnd()));
			SetWindowSubclass(ComboBox->GetHwnd(), (SUBCLASSPROC)NodeComboBoxProc, 0, (DWORD_PTR)this);
			ComboBox->Init(ComboBoxItems[0], ComboBoxItems, ComboBoxProp.MinVisibleItem);

			WidthMargin += ComboBoxProp.MarginX;
			MaxWidth += ComboBoxProp.InitWidth;
			MaxHeight += 10;
		}
	}

	// add input pins
	{
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(Inputs.size()); Idx++)
		{
			SIZE TextSize;
			GetTextExtentPoint32A(NodeDC, Inputs[Idx]->GetName().c_str(), (int32_t)Inputs[Idx]->GetName().length(), &TextSize);

			// Add pin radio button
			{
				UniquePtr<UHRadioButton> NewPin = MakeUnique<UHRadioButton>(Inputs[Idx]->GetName()
					, UHGUIProperty().SetInstance(InInstance).SetParent(NodeGUI->GetHwnd())
					.SetPosX(5)
					.SetPosY(15 + Idx * (TextSize.cy + 2))
					.SetWidth(TextSize.cx + 30)
					.SetHeight(TextSize.cy), false);

				// register window callback for pins
				SetWindowSubclass(NewPin->GetHwnd(), (SUBCLASSPROC)NodeInputPinProc, 0, (DWORD_PTR)Inputs[Idx].get());

				MaxWidth = (std::max)(MaxWidth + WidthMargin, (int32_t)TextSize.cx + 30);
				MaxHeight += (TextSize.cy + 2);

				Inputs[Idx]->SetPinGUI(NewPin.get());
				ShowWindow(NewPin->GetHwnd(), SW_SHOW);
				InputsButton.push_back(std::move(NewPin));
			}

			// add edit control if it's requested
			if (InputTextFieldLength > 0 && Inputs[Idx]->NeedInputField())
			{
				UniquePtr<UHTextBox> NewEdit = MakeUnique<UHTextBox>("0", UHGUIProperty().SetInstance(InInstance).SetParent(NodeGUI->GetHwnd())
					.SetPosX(5 + TextSize.cx + 30)
					.SetPosY(15 + Idx * (TextSize.cy + 2))
					.SetWidth(InputTextFieldLength)
					.SetHeight(TextSize.cy));

				MaxWidth = (std::max)(MaxWidth, (int32_t)TextSize.cx + 40 + InputTextFieldLength);
				ShowWindow(NewEdit->GetHwnd(), SW_SHOW);
				InputsTextFields.push_back(std::move(NewEdit));
			}
		}
	}

	// add output pins
	int32_t OutputStartX = MaxWidth + WidthMargin;
	int32_t MaxHeightOutput = 0;
	{
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(Outputs.size()); Idx++)
		{
			SIZE TextSize;
			GetTextExtentPoint32A(NodeDC, Outputs[Idx]->GetName().c_str(), (int32_t)Outputs[Idx]->GetName().length(), &TextSize);

			UniquePtr<UHRadioButton> NewPin = MakeUnique<UHRadioButton>(Outputs[Idx]->GetName()
				, UHGUIProperty().SetInstance(InInstance).SetParent(NodeGUI->GetHwnd())
				.SetPosX(OutputStartX)
				.SetPosY(15 + Idx * (TextSize.cy + 2))
				.SetWidth(TextSize.cx + 30)
				.SetHeight(TextSize.cy), true);

			// register window callback for pins
			SetWindowSubclass(NewPin->GetHwnd(), (SUBCLASSPROC)NodeOutputPinProc, 0, (DWORD_PTR)Outputs[Idx].get());

			MaxWidth = (std::max)(MaxWidth, (int32_t)TextSize.cx + 30 + OutputStartX);
			MaxHeightOutput += (TextSize.cy + 2);

			Outputs[Idx]->SetPinGUI(NewPin.get());
			ShowWindow(NewPin->GetHwnd(), SW_SHOW);
			OutputsButton.push_back(std::move(NewPin));
		}
	}
	MaxHeight = (std::max)(MaxHeight, MaxHeightOutput);

	this->PostAddingPins();
	MoveWindow(NodeGUI->GetHwnd(), PosX, PosY, MaxWidth + 10, MaxHeight + 20, false);
	ShowWindow(NodeGUI->GetHwnd(), SW_SHOW);
	ReleaseDC(NodeGUI->GetHwnd(), NodeDC);
}

void UHGraphNodeGUI::Release()
{
	NodeGUI = nullptr;
	InputsButton.clear();
	InputsTextFields.clear();
	OutputsButton.clear();
	ComboBoxItems.clear();
}

bool UHGraphNodeGUI::IsPointInside(POINT InMousePos) const
{
	return NodeGUI->IsPointInside(InMousePos);
}

UHGraphPin* UHGraphNodeGUI::GetInputPinByMousePos(POINT InMousePos, int32_t& OutIndex) const
{
	if (NodeGUI->IsPointInside(InMousePos))
	{
		std::vector<UniquePtr<UHGraphPin>>& InputPins = GraphNode->GetInputs();
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(InputsButton.size()); Idx++)
		{
			if (InputsButton[Idx]->IsPointInside(InMousePos))
			{
				OutIndex = Idx;
				return InputPins[Idx].get();
			}
		}
	}

	return nullptr;
}

HWND UHGraphNodeGUI::GetHWND() const
{
	return NodeGUI->GetHwnd();
}

UHRadioButton* UHGraphNodeGUI::GetInputPin(int32_t InIndex) const
{
	if (InIndex >= static_cast<int32_t>(InputsButton.size()))
	{
		return nullptr;
	}

	return InputsButton[InIndex].get();
}

UHRadioButton* UHGraphNodeGUI::GetOutputPin(int32_t InIndex) const
{
	if (InIndex >= static_cast<int32_t>(OutputsButton.size()))
	{
		return nullptr;
	}

	return OutputsButton[InIndex].get();
}

UHGraphNode* UHGraphNodeGUI::GetNode()
{
	return GraphNode;
}

#endif