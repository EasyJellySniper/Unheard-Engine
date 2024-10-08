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

	virtual void OnCompile() override;

	void BindParameters();

private:
	std::vector<uint32_t> ClosestHitIDs;
	std::vector<uint32_t> AnyHitIDs;
};