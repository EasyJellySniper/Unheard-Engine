#pragma once
#include "ShaderClass.h"

class UHIndirectLightPassShader : public UHShaderClass
{
public:
	UHIndirectLightPassShader(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;

	void BindParameters();
};