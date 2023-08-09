#pragma once
#include "ShaderClass.h"

class UHTranslucentPassShader : public UHShaderClass
{
public:
	UHTranslucentPassShader() {}
	UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& LightConst
		, const UHRenderTexture* RTShadowResult
		, const UHSampler* LinearClamppedSampler
		, const UHMeshRendererComponent* InRenderer);
};