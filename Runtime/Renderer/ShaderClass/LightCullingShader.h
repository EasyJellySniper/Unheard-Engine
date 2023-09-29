#pragma once
#include "ShaderClass.h"

class UHLightCullingShader : public UHShaderClass
{
public:
	UHLightCullingShader(UHGraphic* InGfx, std::string Name);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
		, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLightConst
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightListTrans
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightListTrans
		, const UHRenderTexture* DepthTexture
		, const UHRenderTexture* TransDepthTexture
		, const UHSampler* LinearClampped);
};