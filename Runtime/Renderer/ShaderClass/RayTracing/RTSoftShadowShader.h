#pragma once
#include "../ShaderClass.h"

struct UHRTSoftShadowConstants
{
	int32_t PCSSKernal;
	float PCSSMinPenumbra;
	float PCSSMaxPenumbra;
	float PCSSBlockerDistScale;

	XMFLOAT4 SoftShadowResolution;
};

class UHRTSoftShadowShader : public UHShaderClass
{
public:
	UHRTSoftShadowShader(UHGraphic* InGfx, std::string Name);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();

	UHRenderBuffer<UHRTSoftShadowConstants>* GetConstants(const int32_t FrameIdx) const;

private:
	UniquePtr<UHRenderBuffer<UHRTSoftShadowConstants>> SoftRTShadowConsts[GMaxFrameInFlight];
};