#include "ParameterNode.h"
#include "../Utility.h"

UHFloatNode::UHFloatNode(float Default)
	: UHParameterNode<float>(Default)
{
	Name = "Float";
	NodeType = FloatNode;

	Inputs.resize(1);
	Inputs[0] = MakeUnique<UHGraphPin>("X", this, FloatPin, true);

	Outputs.resize(1);
	Outputs[0] = MakeUnique<UHGraphPin>("Result", this, FloatPin);
}

UHFloat2Node::UHFloat2Node(XMFLOAT2 Default)
	: UHParameterNode<XMFLOAT2>(Default)
{
	Name = "Float2";
	NodeType = Float2Node;

	Inputs.resize(3);
	Inputs[0] = MakeUnique<UHGraphPin>("Input", this, Float2Pin);
	Inputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin, true);
	Inputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin, true);

	Outputs.resize(3);
	Outputs[0] = MakeUnique<UHGraphPin>("Result", this, Float2Pin);
	Outputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin);
	Outputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin);
}

UHFloat3Node::UHFloat3Node(XMFLOAT3 Default)
	: UHParameterNode<XMFLOAT3>(Default)
{
	Name = "Float3";
	NodeType = Float3Node;

	Inputs.resize(4);
	Inputs[0] = MakeUnique<UHGraphPin>("Input", this, Float3Pin);
	Inputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin, true);
	Inputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin, true);
	Inputs[3] = MakeUnique<UHGraphPin>("Z", this, FloatPin, true);

	Outputs.resize(4);
	Outputs[0] = MakeUnique<UHGraphPin>("Result", this, Float3Pin);
	Outputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin);
	Outputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin);
	Outputs[3] = MakeUnique<UHGraphPin>("Z", this, FloatPin);
}

UHFloat4Node::UHFloat4Node(XMFLOAT4 Default)
	: UHParameterNode<XMFLOAT4>(Default)
{
	Name = "Float4";
	NodeType = Float4Node;

	Inputs.resize(5);
	Inputs[0] = MakeUnique<UHGraphPin>("Input", this, Float4Pin);
	Inputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin, true);
	Inputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin, true);
	Inputs[3] = MakeUnique<UHGraphPin>("Z", this, FloatPin, true);
	Inputs[4] = MakeUnique<UHGraphPin>("W", this, FloatPin, true);

	Outputs.resize(5);
	Outputs[0] = MakeUnique<UHGraphPin>("Result", this, Float4Pin);
	Outputs[1] = MakeUnique<UHGraphPin>("X", this, FloatPin);
	Outputs[2] = MakeUnique<UHGraphPin>("Y", this, FloatPin);
	Outputs[3] = MakeUnique<UHGraphPin>("Z", this, FloatPin);
	Outputs[4] = MakeUnique<UHGraphPin>("W", this, FloatPin);
}

std::string UHFloatNode::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// only need the definition for RT, as the parameter defines are in cbuffer for common passes
	if (!bIsCompilingRayTracing)
	{
		return "";
	}

	// E.g. float Node_1234 = asfloat(MatData.Data[3]);
	return "float Node_" + std::to_string(GetId()) + " = asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial) +"]);\n";
}

std::string UHFloat2Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// only need the definition for RT, as the parameter defines are in cbuffer for common passes
	if (!bIsCompilingRayTracing)
	{
		return "";
	}

	// E.g. float2 Node_1234 = float2(asfloat(MatData.Data[3]), asfloat(MatData.Data[4]));
	return "float2 Node_" + std::to_string(GetId()) + " = float2(asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 1) + "]));\n";
}

std::string UHFloat3Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// only need the definition for RT, as the parameter defines are in cbuffer for common passes
	if (!bIsCompilingRayTracing)
	{
		return "";
	}

	return "float3 Node_" + std::to_string(GetId()) + " = float3(asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 1) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 2) + "]));\n";
}

std::string UHFloat4Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// only need the definition for RT, as the parameter defines are in cbuffer for common passes
	if (!bIsCompilingRayTracing)
	{
		return "";
	}

	return "float3 Node_" + std::to_string(GetId()) + " = float3(asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 1) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 2) + "]), "
		+ "asfloat(MatData.Data[" + std::to_string(DataIndexInMaterial + 3) + "]));\n";
}

bool UHFloatNode::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloatNode* FloatPin = dynamic_cast<const UHFloatNode*>(InNode))
	{
		return GetValue() == FloatPin->GetValue();
	}
	return false;
}

bool UHFloat2Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat2Node* Float2Pin = dynamic_cast<const UHFloat2Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float2Pin->GetValue());
	}
	return false;
}

bool UHFloat3Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat3Node* Float3Pin = dynamic_cast<const UHFloat3Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float3Pin->GetValue());
	}
	return false;
}

bool UHFloat4Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat4Node* Float4Pin = dynamic_cast<const UHFloat4Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float4Pin->GetValue());
	}
	return false;
}