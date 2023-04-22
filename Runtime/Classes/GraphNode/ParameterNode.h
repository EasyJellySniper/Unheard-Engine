#pragma once
#include "../../../UnheardEngine.h"
#include "GraphNode.h"
#include "../Types.h"

// UH parameter node structure
template <typename T>
class UHParameterNode : public UHGraphNode
{
public:
	UHParameterNode(T InitValue)
		: UHGraphNode(true)
		, DefaultValue(InitValue)
	{

	}

	void SetValue(T InValue)
	{
		DefaultValue = InValue;
	}

	T GetValue() const
	{
		return DefaultValue;
	}

	virtual std::string EvalHLSL() override
	{
		// return the node name directly, value should be defined in EvalDefinition()
		if (CanEvalHLSL())
		{
			// eval from an exist input
			if (Inputs[0]->GetSrcPin() != nullptr && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
			{
				return Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL();
			}

			return "Node_" + std::to_string(GetId());
		}

		return "";
	}

	virtual void InputData(std::ifstream& FileIn) override
	{
		FileIn.read(reinterpret_cast<char*>(&DefaultValue), sizeof(DefaultValue));
	}

	virtual void OutputData(std::ofstream& FileOut) override
	{
		FileOut.write(reinterpret_cast<const char*>(&DefaultValue), sizeof(DefaultValue));
	}

protected:
	T DefaultValue;
};

class UHFloatNode : public UHParameterNode<float>
{
public:
	UHFloatNode(float Default = 0.0f);
	virtual std::string EvalDefinition() override;
	virtual bool IsEqual(const UHGraphNode* InNode) override;
};

class UHFloat2Node : public UHParameterNode<XMFLOAT2>
{
public:
	UHFloat2Node(XMFLOAT2 Default = XMFLOAT2());
	virtual std::string EvalDefinition() override;
	virtual bool IsEqual(const UHGraphNode* InNode) override;
};

class UHFloat3Node : public UHParameterNode<XMFLOAT3>
{
public:
	UHFloat3Node(XMFLOAT3 Default = XMFLOAT3());
	virtual std::string EvalDefinition() override;
	virtual bool IsEqual(const UHGraphNode* InNode) override;
};

class UHFloat4Node : public UHParameterNode<XMFLOAT4>
{
public:
	UHFloat4Node(XMFLOAT4 Default = XMFLOAT4());
	virtual std::string EvalDefinition() override;
	virtual bool IsEqual(const UHGraphNode* InNode) override;
};