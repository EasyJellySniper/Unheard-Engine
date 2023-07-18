#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

class UHRTShadowShader : public UHShaderClass
{
public:
	UHRTShadowShader() {}

	// this shader needs hit group
	UHRTShadowShader(UHGraphic* InGfx, std::string Name
		, const std::vector<uint32_t>& InClosestHits
		, const std::vector<uint32_t>& InAnyHits
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* RTShadowResult
		, const UHRenderTexture* RTTranslucentShadow
		, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
		, const UHRenderTexture* SceneMip
		, const UHRenderTexture* SceneDepth
		, const UHRenderTexture* TranslucentDepth
		, const UHRenderTexture* VertexNormal
		, const UHRenderTexture* TranslucentVertexNormal
		, const UHSampler* LinearClampedSampler);
};