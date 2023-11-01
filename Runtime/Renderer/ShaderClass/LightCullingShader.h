#pragma once
#include "ShaderClass.h"

class UHLightCullingShader : public UHShaderClass
{
public:
	UHLightCullingShader(UHGraphic* InGfx, std::string Name);
	void BindParameters();
};