#pragma once
#include "ShaderClass.h"

#if WITH_EDITOR
class UHSmoothCubemapShader : public UHShaderClass
{
public:
	UHSmoothCubemapShader(UHGraphic* InGfx, std::string Name);
	virtual void OnCompile() override;
};
#endif