#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

class UHRTReflectionShader : public UHShaderClass
{
public:
	// this shader needs hit group
	UHRTReflectionShader(UHGraphic* InGfx, std::string Name
		, const std::vector<uint32_t>& InClosestHits
		, const std::vector<uint32_t>& InAnyHits
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	virtual void OnCompile() override;

	void BindParameters();
	void BindSkyCube();

	// set max recursion for the reflection, real recursion is still decided by material setting.
	static const uint32_t MaxReflectionRecursion = 8;

private:
	std::vector<uint32_t> ClosestHitIDs;
	std::vector<uint32_t> AnyHitIDs;
};