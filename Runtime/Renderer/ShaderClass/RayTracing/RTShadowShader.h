#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

class UHRTShadowShader : public UHShaderClass
{
public:
	// this shader needs hit group
	UHRTShadowShader(UHGraphic* InGfx, std::string Name
		, const std::vector<uint32_t>& InClosestHits
		, const std::vector<uint32_t>& InAnyHits
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* RTShadowResult
		, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
		, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLights
		, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLights
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightListTrans
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightListTrans
		, const UHRenderTexture* SceneMip
		, const UHRenderTexture* SceneDepth
		, const UHRenderTexture* TranslucentDepth
		, const UHRenderTexture* VertexNormal
		, const UHRenderTexture* TranslucentVertexNormal
		, const UHSampler* LinearClampedSampler);
};