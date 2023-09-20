#pragma once
#include "../ShaderClass.h"

class UHTemporalAAShader : public UHShaderClass
{
public:
	UHTemporalAAShader(UHGraphic* InGfx, std::string Name);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* PreviousSceneResult
		, const UHRenderTexture* MotionVectorRT
		, const UHRenderTexture* PrevMotionVectorRT
		, const UHSampler* LinearClampedSampler);
};