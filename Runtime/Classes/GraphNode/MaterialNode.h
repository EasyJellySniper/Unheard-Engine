#pragma once
#include "GraphNode.h"
#include "../MaterialLayout.h"

// material nodes class
class UHMaterialNode : public UHGraphNode
{
public:
	UHMaterialNode();
	virtual std::string EvalHLSL() override;
	virtual void InputData(std::ifstream& FileIn) override {}
	virtual void OutputData(std::ofstream& FileOut) override {}
};

extern std::unique_ptr<UHGraphNode> AllocateNewGraphNode(UHGraphNodeType InType);