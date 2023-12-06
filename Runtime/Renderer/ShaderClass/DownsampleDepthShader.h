#pragma once
#include "ShaderClass.h"

class UHDownsampleDepthShader : public UHShaderClass
{
public:
	UHDownsampleDepthShader(UHGraphic* InGfx, std::string Name);
	virtual void OnCompile() override;
	void BindParameters();
};