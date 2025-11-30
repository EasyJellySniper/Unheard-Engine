#pragma once
#include "../ShaderClass.h"

struct UHSoftRTShadowConstants
{
	int32_t PCSSKernal;
	float PCSSMinPenumbra;
	float PCSSMaxPenumbra;
	float PCSSBlockerDistScale;
};

class UHSoftRTShadowShader : public UHShaderClass
{
public:
	UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();

	UHRenderBuffer<UHSoftRTShadowConstants>* GetConstants(const int32_t FrameIdx) const;

private:
	UniquePtr<UHRenderBuffer<UHSoftRTShadowConstants>> SoftRTShadowConsts[GMaxFrameInFlight];
};