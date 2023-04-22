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

	void SetSelectedTextureName(std::string InSelectedTextureName);
	std::string GetSelectedTextureName() const;

private:
	std::string SelectedTextureName;
	bool bIsBumpTexture;
};