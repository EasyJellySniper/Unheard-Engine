#pragma once
#include "../../../../UnheardEngine.h"
#include "../../../../Runtime/Classes/Object.h"
#if WITH_EDITOR
#include "../../../Editor/Controls/RadioButton.h"
#endif

enum UHGraphPinType
{
    VoidPin,
    FloatPin,
    Float2Pin,
    Float3Pin,
    Float4Pin,

    // any type node will be checked when compilation
    AnyPin
};

class UHGraphNode;

// Graph Pin Class in UHE, nullptr of node means it's not connected
class UHGraphPin : public UHObject
{
public:
    UHGraphPin(std::string InName, UHGraphNode* InNode, UHGraphPinType InType, bool bInNeedInputField = false);
    ~UHGraphPin();

#if WITH_EDITOR
    void SetPinGUI(UHRadioButton* InGUI);
    UHRadioButton* GetPinGUI() const;
#endif
    bool ConnectFrom(UHGraphPin* InSrcPin);
    void ConnectTo(UHGraphPin* InDestPin);
    void Disconnect(uint32_t DestPinID = UINT32_MAX);

    std::string GetName() const;
    UHGraphNode* GetOriginNode() const;
    UHGraphPinType GetType() const;
    UHGraphPin* GetSrcPin() const;
    std::vector<UHGraphPin*>& GetDestPins();
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
#if WITH_EDITOR
    UHRadioButton* PinGUI;
#endif

    // if need input text field
    bool bNeedInputField;
};