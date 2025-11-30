#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

struct UHRTIndirectLightConstants
{
	float IndirectLightScale;
	float IndirectLightFadeDistance;
	float IndirectLightMaxTraceDist;
	float IndirectOcclusionDistance;
	int32_t IndirectDownFactor;
	uint32_t IndirectRTSize;
};

class UHRTIndirectLightShader : public UHShaderClass
{
public:
	// this shader needs hit group
	UHRTIndirectLightShader(UHGraphic* InGfx, std::string Name
		, const std::vector<uint32_t>& InClosestHits
		, const std::vector<uint32_t>& InAnyHits
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();

	UHRenderBuffer<UHRTIndirectLightConstants>* GetConstants(const int32_t FrameIdx) const;

	// set max recursion for the indirect light, real recursion is still decided by the settings.
	static const uint32_t MaxIndirectLightRecursion = 8;

private:
	std::vector<uint32_t> ClosestHitIDs;
	std::vector<uint32_t> AnyHitIDs;

	UniquePtr<UHRenderBuffer<UHRTIndirectLightConstants>> RTIndirectLightConstants[GMaxFrameInFlight];
};