#pragma once
#include "ShaderClass.h"

class UHSphericalHarmonicShader : public UHShaderClass
{
public:
	UHSphericalHarmonicShader(UHGraphic* InGfx, std::string Name);
	virtual void Release() override;
	virtual void OnCompile() override;

	UHRenderBuffer<UHSphericalHarmonicConstants>* GetSH9Constants(const int32_t FrameIdx) const;
	void BindParameters();
private:
	UniquePtr<UHRenderBuffer<UHSphericalHarmonicConstants>> SH9Constants[GMaxFrameInFlight];
};