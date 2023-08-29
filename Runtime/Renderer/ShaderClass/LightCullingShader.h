#pragma once
#include "ShaderClass.h"

class UHLightCullingShader : public UHShaderClass
{
public:
	UHLightCullingShader() {}
	UHLightCullingShader(UHGraphic* InGfx, std::string Name);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightListTrans
		, const UHRenderTexture* DepthTexture
		, const UHRenderTexture* TransDepthTexture
		, const UHSampler* LinearClampped);
};