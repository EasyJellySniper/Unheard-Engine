#pragma once
#include "ShaderClass.h"

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;

	void BindParameters();
};