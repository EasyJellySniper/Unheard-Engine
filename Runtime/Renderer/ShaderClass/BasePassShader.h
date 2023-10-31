#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass, bool bHasEnvCube
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const UHMeshRendererComponent* InRenderer
		, const UHTextureCube* EnvCube
		, const UHSampler* EnvSampler);
};