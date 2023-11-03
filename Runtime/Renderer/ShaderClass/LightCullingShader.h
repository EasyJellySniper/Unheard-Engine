#pragma once
#include "ShaderClass.h"

class UHLightCullingShader : public UHShaderClass
{
public:
	UHLightCullingShader(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;

	void BindParameters();
};