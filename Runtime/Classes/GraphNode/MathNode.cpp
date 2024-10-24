#include "MathNode.h"

UHMathNode::UHMathNode(UHMathNodeOperator DefaultOp)
	: UHGraphNode(true)
{
	Name = "Math";
	NodeType = UHGraphNodeType::MathNode;

	Inputs.resize(2);
	Inputs[0] = MakeUnique<UHGraphPin>("A", this, UHGraphPinType::AnyPin);
	Inputs[1] = MakeUnique<UHGraphPin>("B", this, UHGraphPinType::AnyPin);

	Outputs.resize(1);
	Outputs[0] = MakeUnique<UHGraphPin>("Result", this, UHGraphPinType::AnyPin);

	CurrentOperator = DefaultOp;
}

bool UHMathNode::CanEvalHLSL()
{
	if (Inputs[0]->GetSrcPin() == nullptr || Inputs[1]->GetSrcPin() == nullptr)
	{
		return false;
	}

	return GetOutputPinType() != UHGraphPinType::VoidPin;
}

std::string UHMathNode::EvalHLSL(const UHGraphPin* CallerPin)
{
	if (CanEvalHLSL())
	{
		std::string Operators[] = { " + "," - "," * "," / " };
		return "(" +  Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL(Inputs[0]->GetSrcPin()) + Operators[UH_ENUM_VALUE(CurrentOperator)] 
			+ Inputs[1]->GetSrcPin()->GetOriginNode()->EvalHLSL(Inputs[1]->GetSrcPin()) + ")";
	}

	return "[ERROR] Type mismatch or input not connected.";
}

void UHMathNode::InputData(std::ifstream& FileIn)
{
	FileIn.read(reinterpret_cast<char*>(&CurrentOperator), sizeof(CurrentOperator));
}

void UHMathNode::OutputData(std::ofstream& FileOut)
{
	FileOut.write(reinterpret_cast<const char*>(&CurrentOperator), sizeof(CurrentOperator));
}

void UHMathNode::SetOperator(UHMathNodeOperator InOp)
{
	CurrentOperator = InOp;
}

UHMathNodeOperator UHMathNode::GetOperator() const
{
	return CurrentOperator;
}

UHGraphPinType UHMathNode::GetOutputPinType() const
{
	// return void if it's not possible to tell which type is
	if (Inputs[0]->GetSrcPin() == nullptr || Inputs[1]->GetSrcPin() == nullptr)
	{
		return UHGraphPinType::VoidPin;
	}

	UHGraphPinType Input0Type = Inputs[0]->GetSrcPin()->GetType();
	UHGraphPinType Input1Type = Inputs[1]->GetSrcPin()->GetType();

	// find real type if "any node" is presented
	if (Input0Type == UHGraphPinType::AnyPin)
	{
		if (UHMathNode* MathNode = static_cast<UHMathNode*>(Inputs[0]->GetSrcPin()->GetOriginNode()))
		{
			Input0Type = MathNode->GetOutputPinType();
		}
	}

	if (Input1Type == UHGraphPinType::AnyPin)
	{
		if (UHMathNode* MathNode = static_cast<UHMathNode*>(Inputs[1]->GetSrcPin()->GetOriginNode()))
		{
			Input1Type = MathNode->GetOutputPinType();
		}
	}

	if (Input0Type == Input1Type)
	{
		return Input0Type;
	}
	else if (Input0Type == UHGraphPinType::FloatPin || Input1Type == UHGraphPinType::FloatPin)
	{
		// bypass the format mismatch if one of them is a float node
		// Per-Component Math Operations is allowed in HLSL
		// E.g. float3(1,1,1) * 3 is possible
		return UHGraphPinType::FloatPin;
	}

	// can not evaluate
	return UHGraphPinType::VoidPin;
}