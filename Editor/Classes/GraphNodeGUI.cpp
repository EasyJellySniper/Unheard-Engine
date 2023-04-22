#include "GraphNodeGUI.h"

#if WITH_DEBUG
#include "../../Runtime/Classes/Utility.h"
#include "EditorUtils.h"
#include <algorithm>
#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

std::unique_ptr<UHPinSelectInfo> GPinSelectInfo;

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

// combo box callback
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
	NodeGUI = CreateWindowA("BUTTON", InGUIName.c_str(), WS_CHILD | BS_GROUPBOX | BS_CENTER
		, PosX, PosY, 10, 10, InParent, nullptr, InInstance, nullptr);

	// call back before adding pins
	this->PreAddingPins();

	std::vector<std::unique_ptr<UHGraphPin>>& Inputs = InNode->GetInputs();
	std::vector<std::unique_ptr<UHGraphPin>>& Outputs = InNode->GetOutputs();

	HDC NodeDC = GetDC(NodeGUI);

	int32_t MaxWidth = 0;
	int32_t MaxHeight = 0;
	int32_t WidthMargin = 0;

	// add combobox
	{
		if (ComboBoxItems.size() > 0)
		{
			ComboBox = CreateWindow(WC_COMBOBOX, L"",
				CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_VSCROLL,
				ComboBoxMeasure.PosX, ComboBoxMeasure.PosY, ComboBoxMeasure.Width, ComboBoxMeasure.Height, NodeGUI, nullptr, InInstance, nullptr);

			SetWindowSubclass(ComboBox, (SUBCLASSPROC)NodeComboBoxProc, 0, (DWORD_PTR)this);
			UHEditorUtil::InitComboBox(ComboBox, ComboBoxItems[0], ComboBoxItems, ComboBoxMeasure.MinVisibleItem);

			WidthMargin += ComboBoxMeasure.Margin;
			MaxWidth += ComboBoxMeasure.Width;
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
				HWND NewPin = CreateWindowA("BUTTON", Inputs[Idx]->GetName().c_str()
					, BS_RADIOBUTTON | WS_CHILD, 5, 15 + Idx * (TextSize.cy + 2), TextSize.cx + 30, TextSize.cy, NodeGUI, NULL, InInstance, NULL);

				// register window callback for pins
				SetWindowSubclass(NewPin, (SUBCLASSPROC)NodeInputPinProc, 0, (DWORD_PTR)Inputs[Idx].get());

				MaxWidth = (std::max)(MaxWidth + WidthMargin, (int32_t)TextSize.cx + 30);
				MaxHeight += (TextSize.cy + 2);

				Inputs[Idx]->SetPinGUI(NewPin);
				InputsButton.push_back(NewPin);
				ShowWindow(NewPin, SW_SHOW);
			}

			// add edit control if it's requested
			if (InputTextFieldLength > 0 && Inputs[Idx]->NeedInputField())
			{
				HWND NewEdit = CreateWindowA("EDIT", "0", WS_BORDER | WS_CHILD, 5 + TextSize.cx + 30, 15 + Idx * (TextSize.cy + 2)
					, InputTextFieldLength, TextSize.cy, NodeGUI, NULL, InInstance, NULL);

				MaxWidth = (std::max)(MaxWidth, (int32_t)TextSize.cx + 40 + InputTextFieldLength);
				InputsTextField.push_back(NewEdit);
				ShowWindow(NewEdit, SW_SHOW);
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

			HWND NewPin = CreateWindowA("BUTTON", Outputs[Idx]->GetName().c_str()
				, BS_RADIOBUTTON | WS_CHILD | BS_LEFTTEXT
				, OutputStartX, 15 + Idx * (TextSize.cy + 2), TextSize.cx + 30, TextSize.cy, NodeGUI, NULL, InInstance, NULL);

			// register window callback for pins
			SetWindowSubclass(NewPin, (SUBCLASSPROC)NodeOutputPinProc, 0, (DWORD_PTR)Outputs[Idx].get());

			MaxWidth = (std::max)(MaxWidth, (int32_t)TextSize.cx + 30 + OutputStartX);
			MaxHeightOutput += (TextSize.cy + 2);

			Outputs[Idx]->SetPinGUI(NewPin);
			OutputsButton.push_back(NewPin);
			ShowWindow(NewPin, SW_SHOW);
		}
	}
	MaxHeight = (std::max)(MaxHeight, MaxHeightOutput);

	this->PostAddingPins();
	MoveWindow(NodeGUI, PosX, PosY, MaxWidth + 10, MaxHeight + 20, false);
	ShowWindow(NodeGUI, SW_SHOW);
	ReleaseDC(NodeGUI, NodeDC);
}

void UHGraphNodeGUI::Release()
{
	for (HWND Hwnd : InputsButton)
	{
		DestroyWindow(Hwnd);
	}

	for (HWND Hwnd : InputsTextField)
	{
		DestroyWindow(Hwnd);
	}

	for (HWND Hwnd : OutputsButton)
	{
		DestroyWindow(Hwnd);
	}
	DestroyWindow(ComboBox);
	DestroyWindow(NodeGUI);

	NodeGUI = nullptr;
	InputsButton.clear();
	InputsTextField.clear();
	OutputsButton.clear();
	ComboBoxItems.clear();
}

bool UHGraphNodeGUI::IsPointInside(POINT InMousePos) const
{
	if (UHEditorUtil::IsPointInsideClient(NodeGUI, InMousePos))
	{
		return true;
	}

	return false;
}

UHGraphPin* UHGraphNodeGUI::GetInputPinByMousePos(POINT InMousePos, int32_t& OutIndex) const
{
	if (UHEditorUtil::IsPointInsideClient(NodeGUI, InMousePos))
	{
		std::vector<std::unique_ptr<UHGraphPin>>& InputPins = GraphNode->GetInputs();
		for (int32_t Idx = 0; Idx < static_cast<int32_t>(InputsButton.size()); Idx++)
		{
			if (UHEditorUtil::IsPointInsideClient(InputsButton[Idx], InMousePos))
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
	return NodeGUI;
}

HWND UHGraphNodeGUI::GetInputPin(int32_t InIndex) const
{
	if (InIndex >= static_cast<int32_t>(InputsButton.size()))
	{
		return nullptr;
	}

	return InputsButton[InIndex];
}

HWND UHGraphNodeGUI::GetOutputPin(int32_t InIndex) const
{
	if (InIndex >= static_cast<int32_t>(OutputsButton.size()))
	{
		return nullptr;
	}

	return OutputsButton[InIndex];
}

HWND UHGraphNodeGUI::GetCombobox() const
{
	return ComboBox;
}

UHGraphNode* UHGraphNodeGUI::GetNode()
{
	return GraphNode;
}

#endif