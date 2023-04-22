#include "MathNode.h"

UHMathNode::UHMathNode(UHMathNodeOperator DefaultOp)
	: UHGraphNode(true)
{
	Name = "Math";
	NodeType = MathNode;

	Inputs.resize(2);
	Inputs[0] = std::make_unique<UHGraphPin>("A", this, AnyNode);
	Inputs[1] = std::make_unique<UHGraphPin>("B", this, AnyNode);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("Result", this, AnyNode);

	CurrentOperator = DefaultOp;
}

bool UHMathNode::CanEvalHLSL()
{
	if (Inputs[0]->GetSrcPin() == nullptr || Inputs[1]->GetSrcPin() == nullptr)
	{
		return false;
	}

	return GetOutputPinType() != VoidNode;
}

std::string UHMathNode::EvalHLSL()
{
	if (CanEvalHLSL())
	{
		std::string Operators[] = { " + "," - "," * "," / " };
		return "(" +  Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL() + Operators[CurrentOperator] + Inputs[1]->GetSrcPin()->GetOriginNode()->EvalHLSL() + ")";
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
		return UHGraphPinType::VoidNode;
	}

	UHGraphPinType Input0Type = Inputs[0]->GetSrcPin()->GetType();
	UHGraphPinType Input1Type = Inputs[1]->GetSrcPin()->GetType();

	// find real type if "any node" is presented
	if (Input0Type == AnyNode)
	{
		if (UHMathNode* MathNode = static_cast<UHMathNode*>(Inputs[0]->GetSrcPin()->GetOriginNode()))
		{
			Input0Type = MathNode->GetOutputPinType();
		}
	}

	if (Input1Type == AnyNode)
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
	else if (Input0Type == FloatNode || Input1Type == FloatNode)
	{
		// bypass the format mismatch if one of them is a float node
		// Per-Component Math Operations is allowed in HLSL
		// E.g. float3(1,1,1) * 3 is possible
		return UHGraphPinType::FloatNode;
	}

	// can not evaluate
	return UHGraphPinType::VoidNode;
}