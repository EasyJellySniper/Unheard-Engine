#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

class UHRTShadowShader : public UHShaderClass
{
public:
	UHRTShadowShader() {}

	// this shader needs hit group
	UHRTShadowShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, const std::vector<UHShader*>& InAnyHits
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* RTShadowResult
		, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
		, const UHRenderTexture* SceneMip
		, const UHRenderTexture* SceneNormal
		, const UHRenderTexture* SceneDepth
		, const UHSampler* PointClampedSampler
		, const UHSampler* LinearClampedSampler);
};