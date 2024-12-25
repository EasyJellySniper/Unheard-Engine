#pragma once
#include "GraphNode.h"
#include "../MaterialLayout.h"

class UHMaterial;
struct UHRTMaterialData;

enum class UHMaterialInputType
{
	MaterialInputStandard,
	MaterialInputOpacityOnly,
	MaterialInputOpacityNormalRoughOnly,
	MaterialInputNormalOnly,
	MaterialInputEmissiveOnly,
	MaterialInputSmoothnessOnly,
	MaterialInputMax,
};

struct UHMaterialCompileData
{
public:
	UHMaterialCompileData()
		: MaterialCache(nullptr)
		, InputType(UHMaterialInputType::MaterialInputStandard)
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

	void InsertOpacityCode(std::string& Code) const;
	void InsertNormalCode(std::string& Code) const;
	void InsertRoughnessCode(std::string& Code) const;
	void InsertEmissiveCode(std::string& Code) const;
	 
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