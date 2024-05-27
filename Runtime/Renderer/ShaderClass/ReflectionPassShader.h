#pragma once
#include "ShaderClass.h"

class UHReflectionPassShader : public UHShaderClass
{
public:
	UHReflectionPassShader(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;

	void BindParameters(const bool bIsRaytracingEnableRT);
	void BindSkyCube();
};