#pragma once
#include "ShaderClass.h"

class UHTranslucentPassShader : public UHShaderClass
{
public:
	UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLightConst
		, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
		, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLightConst
		, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
		, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
		, const UHRenderTexture* RTShadowResult
		, const UHSampler* LinearClamppedSampler
		, const UHMeshRendererComponent* InRenderer
		, const int32_t RTInstanceCount);
};