#pragma once
#include "ShaderClass.h"

struct UHSoftShadowConstants
{
	int32_t PCSSKernal;
	float PCSSMinPenumbra;
	float PCSSMaxPenumbra;
	float PCSSBlockerDistScale;
};

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader(UHGraphic* InGfx, std::string Name);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters(const bool bIsRaytracingEnableRT);

	UHRenderBuffer<UHSoftShadowConstants>* GetConstants(const int32_t FrameIdx) const;

private:
	UniquePtr<UHRenderBuffer<UHSoftShadowConstants>> SoftShadowConsts[GMaxFrameInFlight];
};