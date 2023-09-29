#pragma once
#include "ShaderClass.h"

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader(UHGraphic* InGfx, std::string Name, int32_t RTInstanceCount);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLightConst
		, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
		, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLightConst
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
		, const std::vector<UHTexture*>& GBuffers
		, const UHRenderTexture* SceneResult
		, const UHSampler* LinearClamppedSampler
		, const int32_t RTInstanceCount
		, const UHRenderTexture* RTShadowResult);
};