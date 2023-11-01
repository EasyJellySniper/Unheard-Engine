#pragma once
#include "../ShaderClass.h"

class UHSoftRTShadowShader : public UHShaderClass
{
public:
	UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name);

	void BindParameters();
};