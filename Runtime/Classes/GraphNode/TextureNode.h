#pragma once
#include "../../../UnheardEngine.h"
#include "GraphNode.h"

class UHTexture2DNode : public UHGraphNode
{
public:
	UHTexture2DNode(std::string TexName = "");
	virtual bool CanEvalHLSL() override;
	virtual std::string EvalDefinition() override;
	virtual std::string EvalHLSL() override;
	virtual void InputData(std::ifstream& FileIn) override;
	virtual void OutputData(std::ofstream& FileOut) override;

	void SetSelectedTexturePathName(std::string InSelectedTextureName);
	std::string GetSelectedTexturePathName() const;
	void SetTextureIndexInMaterial(int32_t InIndex);

private:
	int32_t TextureIndexInMaterial;
	std::string SelectedTexturePathName;
};