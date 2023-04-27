#pragma once
#include "GraphNode.h"
#include "../MaterialLayout.h"

class UHMaterial;

struct UHMaterialCompileData
{
public:
	UHMaterialCompileData()
		: MaterialCache(nullptr)
		, bIsDepthOrMotionPass(false)
	{
	}

	UHMaterial* MaterialCache;
	bool bIsDepthOrMotionPass;
};

// material nodes class
class UHMaterialNode : public UHGraphNode
{
public:
	UHMaterialNode(UHMaterial* InMat);
	virtual std::string EvalHLSL() override;
	virtual void InputData(std::ifstream& FileIn) override {}
	virtual void OutputData(std::ofstream& FileOut) override {}

	void SetMaterialCompileData(UHMaterialCompileData InData);

private:
	UHMaterial* MaterialCache;
	UHMaterialCompileData CompileData;
};

extern std::unique_ptr<UHGraphNode> AllocateNewGraphNode(UHGraphNodeType InType);