#include "GraphPin.h"
#include "GraphNode.h"

UHGraphPin::UHGraphPin(std::string InName, UHGraphNode* InNode, UHGraphPinType InType, bool bInNeedInputField)
	: Name(InName)
	, OriginNode(InNode)
	, SrcPin(nullptr)
	, Type(InType)
	, bNeedInputField(bInNeedInputField)
	, PinGUI(nullptr)
{

}

UHGraphPin::~UHGraphPin()
{
	DestPins.clear();
}

void UHGraphPin::SetPinGUI(HWND InGUI)
{
	PinGUI = InGUI;
}

bool UHGraphPin::ConnectFrom(UHGraphPin* InSrcPin)
{
	bool bHasAnyType = Type == AnyNode || InSrcPin->GetType() == AnyNode;
	if (InSrcPin->GetType() != Type && !bHasAnyType)
	{
		UHE_LOG("[ERROR] Type mismatch for the pin " + InSrcPin->GetName() + " and " + Name + "\n");
		return false;
	}

	if (SrcPin)
	{
		// disconnect old src pin
		SrcPin->Disconnect(GetId());
	}

	// store the source pin connected from
	// also need connect src to dst (this)
	SrcPin = InSrcPin;
	SrcPin->ConnectTo(this);

	return true;
}

void UHGraphPin::ConnectTo(UHGraphPin* InDestPin)
{
	// do not duplicate pin
	for (const UHGraphPin* DestPin : DestPins)
	{
		if (DestPin->GetId() == InDestPin->GetId())
		{
			return;
		}
	}

	DestPins.push_back(InDestPin);
}

void UHGraphPin::Disconnect(uint32_t DestPinID)
{
	for (int32_t Idx = static_cast<int32_t>(DestPins.size() - 1); Idx >= 0; Idx--)
	{
		if (DestPins[Idx]->GetId() == DestPinID)
		{
			DestPins.erase(DestPins.begin() + Idx);
			break;
		}
	}

	SrcPin = nullptr;
}

std::string UHGraphPin::GetName() const
{
	return Name;
}

UHGraphNode* UHGraphPin::GetOriginNode() const
{
	return OriginNode;
}

UHGraphPinType UHGraphPin::GetType() const
{
	return Type;
}

UHGraphPin* UHGraphPin::GetSrcPin() const
{
	return SrcPin;
}

std::vector<UHGraphPin*>& UHGraphPin::GetDestPins()
{
	return DestPins;
}

HWND UHGraphPin::GetPinGUI() const
{
	return PinGUI;
}

bool UHGraphPin::NeedInputField() const
{
	return bNeedInputField;
}