#include "ParameterNode.h"
#include "../Utility.h"

UHFloatNode::UHFloatNode(float Default)
	: UHParameterNode<float>(Default)
{
	Name = "Float";
	NodeType = Float;

	Inputs.resize(1);
	Inputs[0] = std::make_unique<UHGraphPin>("X", this, FloatNode, true);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("Result", this, FloatNode);
}

UHFloat2Node::UHFloat2Node(XMFLOAT2 Default)
	: UHParameterNode<XMFLOAT2>(Default)
{
	Name = "Float2";
	NodeType = Float2;

	Inputs.resize(3);
	Inputs[0] = std::make_unique<UHGraphPin>("Input", this, Float2Node);
	Inputs[1] = std::make_unique<UHGraphPin>("X", this, FloatNode, true);
	Inputs[2] = std::make_unique<UHGraphPin>("Y", this, FloatNode, true);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("Result", this, Float2Node);
}

UHFloat3Node::UHFloat3Node(XMFLOAT3 Default)
	: UHParameterNode<XMFLOAT3>(Default)
{
	Name = "Float3";
	NodeType = Float3;

	Inputs.resize(4);
	Inputs[0] = std::make_unique<UHGraphPin>("Input", this, Float3Node);
	Inputs[1] = std::make_unique<UHGraphPin>("X", this, FloatNode, true);
	Inputs[2] = std::make_unique<UHGraphPin>("Y", this, FloatNode, true);
	Inputs[3] = std::make_unique<UHGraphPin>("Z", this, FloatNode, true);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("Result", this, Float3Node);
}

UHFloat4Node::UHFloat4Node(XMFLOAT4 Default)
	: UHParameterNode<XMFLOAT4>(Default)
{
	Name = "Float4";
	NodeType = Float4;

	Inputs.resize(5);
	Inputs[0] = std::make_unique<UHGraphPin>("Input", this, Float4Node);
	Inputs[1] = std::make_unique<UHGraphPin>("X", this, FloatNode, true);
	Inputs[2] = std::make_unique<UHGraphPin>("Y", this, FloatNode, true);
	Inputs[3] = std::make_unique<UHGraphPin>("Z", this, FloatNode, true);
	Inputs[4] = std::make_unique<UHGraphPin>("W", this, FloatNode, true);

	Outputs.resize(1);
	Outputs[0] = std::make_unique<UHGraphPin>("Result", this, Float4Node);
}

std::string UHFloatNode::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// E.g. float Node_1234 = 0.0f;
	return "float Node_" + std::to_string(GetId()) + " = " + UHUtilities::FloatToString(DefaultValue) + "f;\n";
}

std::string UHFloat2Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// consider input node
	const std::string InputX = (Inputs[1]->GetSrcPin() && Inputs[1]->GetSrcPin()->GetOriginNode()) ? Inputs[1]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.x);
	const std::string InputY = (Inputs[2]->GetSrcPin() && Inputs[2]->GetSrcPin()->GetOriginNode()) ? Inputs[2]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.y);

	// E.g. float2 Node_1234 = float2(0.0f, 0.0f);
	return "float2 Node_" + std::to_string(GetId()) + " = float2(" + InputX + "f, " + InputY + "f);\n";
}

std::string UHFloat3Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// consider input node
	const std::string InputX = (Inputs[1]->GetSrcPin() && Inputs[1]->GetSrcPin()->GetOriginNode()) ? Inputs[1]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.x);
	const std::string InputY = (Inputs[2]->GetSrcPin() && Inputs[2]->GetSrcPin()->GetOriginNode()) ? Inputs[2]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.y);
	const std::string InputZ = (Inputs[3]->GetSrcPin() && Inputs[3]->GetSrcPin()->GetOriginNode()) ? Inputs[3]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.z);

	// E.g. float3 Node_1234 = float3(0.0f, 0.0f, 0.0f);
	return "float3 Node_" + std::to_string(GetId()) + " = float3(" + InputX + "f, " + InputY + "f, " + InputZ + "f);\n";
}

std::string UHFloat4Node::EvalDefinition()
{
	// from input, skip definition
	if (Inputs[0]->GetSrcPin() && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
	{
		return "";
	}

	// consider input node
	const std::string InputX = (Inputs[1]->GetSrcPin() && Inputs[1]->GetSrcPin()->GetOriginNode()) ? Inputs[1]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.x);
	const std::string InputY = (Inputs[2]->GetSrcPin() && Inputs[2]->GetSrcPin()->GetOriginNode()) ? Inputs[2]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.y);
	const std::string InputZ = (Inputs[3]->GetSrcPin() && Inputs[3]->GetSrcPin()->GetOriginNode()) ? Inputs[3]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.z);
	const std::string InputW = (Inputs[4]->GetSrcPin() && Inputs[4]->GetSrcPin()->GetOriginNode()) ? Inputs[4]->GetSrcPin()->GetOriginNode()->EvalHLSL() : UHUtilities::FloatToString(DefaultValue.w);

	// E.g. float4 Node_1234 = float4(0,0,0,0);
	return "float4 Node_" + std::to_string(GetId()) + " = float4(" + InputX + "f, " + InputY + "f, " + InputZ + "f, " + InputW + "f);\n";
}

bool UHFloatNode::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloatNode* FloatNode = dynamic_cast<const UHFloatNode*>(InNode))
	{
		return GetValue() == FloatNode->GetValue();
	}
	return false;
}

bool UHFloat2Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat2Node* Float2Node = dynamic_cast<const UHFloat2Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float2Node->GetValue());
	}
	return false;
}

bool UHFloat3Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat3Node* Float3Node = dynamic_cast<const UHFloat3Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float3Node->GetValue());
	}
	return false;
}

bool UHFloat4Node::IsEqual(const UHGraphNode* InNode)
{
	if (const UHFloat4Node* Float4Node = dynamic_cast<const UHFloat4Node*>(InNode))
	{
		return MathHelpers::IsVectorEqual(GetValue(), Float4Node->GetValue());
	}
	return false;
}