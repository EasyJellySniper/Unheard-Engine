#pragma once
#include "../ShaderClass.h"

class UHSoftRTShadowShader : public UHShaderClass
{
public:
	UHSoftRTShadowShader() {}
	UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* RTDirShadowResult
		, const UHRenderTexture* RTPointShadowResult
		, const UHRenderTexture* InputRTShadow
		, const UHRenderTexture* DepthTexture
		, const UHRenderTexture* TranslucentDepth
		, const UHRenderTexture* SceneMip
		, const UHSampler* LinearClamppedSampler);
};