#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <vector>
#include "../../Runtime/Classes/GraphNode/GraphNode.h"
#include "../Controls/GroupBox.h"
#include "../Controls/TextBox.h"
#include "../Controls/ComboBox.h"
#include "../Controls/RadioButton.h"

class UHGraphNodeGUI
{
public:
	UHGraphNodeGUI();
	virtual ~UHGraphNodeGUI();

	// virtual callback functions
	virtual void Update() {}
	virtual void SetDefaultValueFromGUI() {}
	virtual void OnSelectCombobox() {}

	void Init(HINSTANCE InInstance, HWND InParent, UHGraphNode* InNode, std::string InGUIName, int32_t PosX, int32_t PosY);
	void Release();

	// is point inside this GUI?
	bool IsPointInside(POINT InMousePos) const;
	UHGraphPin* GetInputPinByMousePos(POINT InMousePos, int32_t& OutIndex) const;

	HWND GetHWND() const;
	UHRadioButton* GetInputPin(int32_t InIndex) const;
	UHRadioButton* GetOutputPin(int32_t InIndex) const;
	UHGraphNode* GetNode();

protected:
	virtual void PreAddingPins() = 0;
	virtual void PostAddingPins() = 0;
	UniquePtr<UHGroupBox> NodeGUI;
	UHGraphNode* GraphNode;

	// textField input
	int32_t InputTextFieldLength;
	std::vector<UniquePtr<UHTextBox>> InputsTextFields;

	// combo box input
	UniquePtr<UHComboBox> ComboBox;
	std::vector<std::wstring> ComboBoxItems;
	UHGUIProperty ComboBoxProp;

private:
	std::string GUIName;
	std::vector<UniquePtr<UHRadioButton>> InputsButton;
	std::vector<UniquePtr<UHRadioButton>> OutputsButton;
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

extern UniquePtr<UHPinSelectInfo> GPinSelectInfo;

#endif
