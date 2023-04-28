#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

class UHRTOcclusionTestShader : public UHShaderClass
{
public:
	UHRTOcclusionTestShader() {}

	// this shader needs hit group
	UHRTOcclusionTestShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, UHShader* InAnyHit, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst);
};