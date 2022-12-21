#pragma once
#include "ShaderClass.h"

class UHLightPassShader : public UHShaderClass
{
public:
	UHLightPassShader() {}
	UHLightPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, int32_t RTInstanceCount);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& LightConst
		, const std::vector<UHTexture*>& GBuffers
		, const UHSampler* LinearClamppedSampler
		, const int32_t RTInstanceCount
		, const UHRenderTexture* RTShadowResult);
};