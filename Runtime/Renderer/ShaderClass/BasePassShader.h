#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader() {}
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst
		, const UHMeshRendererComponent* InRenderer);
};