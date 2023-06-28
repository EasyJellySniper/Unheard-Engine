#pragma once
#include "../ShaderClass.h"

class UHTemporalAAShader : public UHShaderClass
{
public:
	UHTemporalAAShader() {}
	UHTemporalAAShader(UHGraphic* InGfx, std::string Name);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* PreviousSceneResult
		, const UHRenderTexture* MotionVectorRT
		, const UHRenderTexture* PrevMotionVectorRT
		, const UHSampler* LinearClampedSampler);
};