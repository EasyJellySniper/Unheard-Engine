#pragma once
#include "GraphNode.h"
#include "../MaterialLayout.h"

class UHMaterial;
struct UHRTMaterialData;

enum UHMaterialInputType
{
	MaterialInputStandard,
	MaterialInputSimple,
	MaterialInputOpacityNormalRoughOnly,
	MaterialInputMax,
};

struct UHMaterialCompileData
{
public:
	UHMaterialCompileData()
		: MaterialCache(nullptr)
		, InputType(MaterialInputStandard)
		, bIsHitGroup(false)
	{
	}

	UHMaterial* MaterialCache;
	UHMaterialInputType InputType;
	bool bIsHitGroup;
};

// material nodes class
class UHMaterialNode : public UHGraphNode
{
public:
	UHMaterialNode(UHMaterial* InMat);
	virtual std::string EvalHLSL(const UHGraphPin* CallerPin) override;
	virtual void InputData(std::ifstream& FileIn) override {}
	virtual void OutputData(std::ofstream& FileOut) override {}

	void SetMaterialCompileData(UHMaterialCompileData InData);
	void CollectTextureIndex(std::string& Code, size_t& OutSize);
	void CollectTextureNames(std::vector<std::string>& Names);
	void CollectMaterialParameter(std::string& Code, size_t& OutSize);
	void CopyMaterialParameter(std::vector<uint8_t>& MaterialData, size_t& BufferAddress);
	void CopyRTMaterialParameter(UHRTMaterialData& RTMaterialData, int32_t& DstIndex);

private:
	UHMaterial* MaterialCache;
	UHMaterialCompileData CompileData;
};

extern UniquePtr<UHGraphNode> AllocateNewGraphNode(UHGraphNodeType InType);