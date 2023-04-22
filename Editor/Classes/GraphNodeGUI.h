#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <vector>
#include "../../Runtime/Classes/GraphNode/GraphNode.h"
#include "GUIDefines.h"

class UHGraphNodeGUI
{
public:
	UHGraphNodeGUI();
	~UHGraphNodeGUI();

	// function to set default value from GUI
	virtual void SetDefaultValueFromGUI() {}
	virtual void OnSelectCombobox() {}

	void Init(HINSTANCE InInstance, HWND InParent, UHGraphNode* InNode, std::string InGUIName, int32_t PosX, int32_t PosY);
	void Release();

	// is point inside this GUI?
	bool IsPointInside(POINT InMousePos) const;
	UHGraphPin* GetInputPinByMousePos(POINT InMousePos, int32_t& OutIndex) const;

	HWND GetHWND() const;
	HWND GetInputPin(int32_t InIndex) const;
	HWND GetOutputPin(int32_t InIndex) const;
	HWND GetCombobox() const;
	UHGraphNode* GetNode();

protected:
	virtual void PreAddingPins() = 0;
	virtual void PostAddingPins() = 0;
	HWND NodeGUI;
	UHGraphNode* GraphNode;

	// textField input
	int32_t InputTextFieldLength;
	std::vector<HWND> InputsTextField;

	// combo box input
	UHGUIMeasurement ComboBoxMeasure;
	std::vector<std::string> ComboBoxItems;
	HWND ComboBox;

private:
	std::string GUIName;
	std::vector<HWND> InputsButton;
	std::vector<HWND> OutputsButton;
};

struct UHPinSelectInfo
{
	UHPinSelectInfo()
		: CurrOutputPin(nullptr)
		, RightClickedPin(nullptr)
		, MouseDownPos(POINT())
		, MouseUpPos(POINT())
		, bReadyForConnect(false)
	{
	}

	UHGraphPin* CurrOutputPin;
	UHGraphPin* RightClickedPin;
	POINT MouseDownPos;
	POINT MouseUpPos;
	bool bReadyForConnect;
};

extern std::unique_ptr<UHPinSelectInfo> GPinSelectInfo;

#endif
