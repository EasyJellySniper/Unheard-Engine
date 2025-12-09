#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

struct UHRTIndirectLightConstants
{
	float IndirectLightScale;
	float IndirectLightFadeDistance;
	float IndirectLightMaxTraceDist;
	float OcclusionEndDistance;
	float OcclusionStartDistance;
	int32_t IndirectDownsampleFactor;
	int32_t UseCache;
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
	static const int32_t NumOfIndirectLightFrames = 2;

private:
	std::vector<uint32_t> ClosestHitIDs;
	std::vector<uint32_t> AnyHitIDs;

	UniquePtr<UHRenderBuffer<UHRTIndirectLightConstants>> RTIndirectLightConstants[GMaxFrameInFlight];
};