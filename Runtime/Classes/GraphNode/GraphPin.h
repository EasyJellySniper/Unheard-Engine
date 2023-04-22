#pragma once
#include "../../../UnheardEngine.h"
#include "../../../Runtime/Classes/Object.h"

enum UHGraphPinType
{
    VoidNode,
    FloatNode,
    Float2Node,
    Float3Node,
    Float4Node,

    // any type node will be checked when compilation
    AnyNode
};

class UHGraphNode;

// Graph Pin Class in UHE, nullptr of node means it's not connected
class UHGraphPin : public UHObject
{
public:
    UHGraphPin(std::string InName, UHGraphNode* InNode, UHGraphPinType InType, bool bInNeedInputField = false);
    ~UHGraphPin();

    void SetPinGUI(HWND InGUI);
    bool ConnectFrom(UHGraphPin* InSrcPin);
    void ConnectTo(UHGraphPin* InDestPin);
    void Disconnect(uint32_t DestPinID = UINT32_MAX);

    std::string GetName() const;
    UHGraphNode* GetOriginNode() const;
    UHGraphPinType GetType() const;
    UHGraphPin* GetSrcPin() const;
    std::vector<UHGraphPin*>& GetDestPins();
    HWND GetPinGUI() const;
    bool NeedInputField() const;

private:
    std::string Name;

    // source pin connected from
    UHGraphPin* SrcPin;

    // dest pin connected to, used for deletion
    std::vector<UHGraphPin*> DestPins;

    // origin node that hold this pin
    UHGraphNode* OriginNode;

    // pin type
    UHGraphPinType Type;

    // pin GUI
    HWND PinGUI;

    // if need input text field
    bool bNeedInputField;
};