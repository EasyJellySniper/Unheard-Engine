#pragma once
#include "ShaderClass.h"

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader(UHGraphic* InGfx, std::string Name);
	virtual void OnCompile() override;

	void BindParameters(const bool bIsRaytracingEnableRT);

	// this must sync with shader implementation
	static const uint32_t MaxSoftShadowLightsPerPixel = 8;
};