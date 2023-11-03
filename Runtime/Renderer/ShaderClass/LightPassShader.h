#pragma once
#include "ShaderClass.h"

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader(UHGraphic* InGfx, std::string Name, int32_t RTInstanceCount);

	virtual void OnCompile() override;

	void BindParameters();

private:
	int32_t RTObjectCount;
};