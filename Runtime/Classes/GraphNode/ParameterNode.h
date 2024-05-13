#pragma once
#include "UnheardEngine.h"
#include "GraphNode.h"
#include "../Types.h"
#include "../MaterialLayout.h"

// UH parameter node structure
template <typename T>
class UHParameterNode : public UHGraphNode
{
public:
	UHParameterNode(T InitValue)
		: UHGraphNode(true)
		, DefaultValue(InitValue)
		, DataIndexInMaterial(-1)
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

	virtual std::string EvalHLSL(const UHGraphPin* CallerPin) override
	{
		// return the node name directly, value should be defined in EvalDefinition()
		if (CanEvalHLSL())
		{
			// eval from an exist input
			if (Inputs[0]->GetSrcPin() != nullptr && Inputs[0]->GetSrcPin()->GetOriginNode() != nullptr)
			{
				return Inputs[0]->GetSrcPin()->GetOriginNode()->EvalHLSL(Inputs[0]->GetSrcPin());
			}

			const int32_t ChannelIdx = FindOutputPinIndexInternal(CallerPin);
			if constexpr (std::is_same<T, float>::value)
			{
				// float parameter has only one channel, early return it
				return "Node_" + std::to_string(GetId());
			}

			const std::string ThisNode = "Node_" + std::to_string(GetId());
			const std::string Channels[] = { ".r",".g",".b",".a" };
			std::string OutputFloat = "float" + std::to_string(Inputs.size() - 1) + "(";

			// set the individual channel if src pin is connect
			for (int32_t Idx = 1; Idx < Inputs.size(); Idx++)
			{
				if (Inputs[Idx]->GetSrcPin() != nullptr && Inputs[Idx]->GetSrcPin()->GetOriginNode() != nullptr)
				{
					OutputFloat += Inputs[Idx]->GetSrcPin()->GetOriginNode()->EvalHLSL(Inputs[Idx]->GetSrcPin());
				}
				else
				{
					OutputFloat += ThisNode + Channels[Idx - 1];
				}

				if (Idx != (int32_t)Inputs.size() - 1)
				{
					OutputFloat += ", ";
				}
			}

			OutputFloat += ")" + GOutChannelShared[ChannelIdx];
			return OutputFloat;
			return "Node_" + std::to_string(GetId()) + GOutChannelShared[ChannelIdx];
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

	void SetDataIndexInMaterial(int32_t InIndex)
	{
		DataIndexInMaterial = InIndex;
	}

protected:
	T DefaultValue;
	int32_t DataIndexInMaterial;
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