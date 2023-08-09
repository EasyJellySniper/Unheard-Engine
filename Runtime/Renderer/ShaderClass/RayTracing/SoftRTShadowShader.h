#pragma once
#include "../ShaderClass.h"

class UHSoftRTShadowShader : public UHShaderClass
{
public:
	UHSoftRTShadowShader() {}
	UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* RTShadowResult
		, const UHRenderTexture* RTTranslucentShadowResult
		, const UHRenderTexture* DepthTexture
		, const UHRenderTexture* TranslucentDepth
		, const UHRenderTexture* SceneMip
		, const UHSampler* LinearClamppedSampler);
};