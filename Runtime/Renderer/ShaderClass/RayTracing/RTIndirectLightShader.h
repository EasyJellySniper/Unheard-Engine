#pragma once
#include "../ShaderClass.h"
#include "RTShaderDefines.h"

struct UHRTIndirectLightConstants
{
	float IndirectLightScale;
	float IndirectLightFadeDistance;
	float IndirectLightMaxTraceDist;
	float MinSkyConeAngle;

	float MaxSkyConeAngle;
	int32_t IndirectDownsampleFactor;
	float IndirectRayAngle;
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

private:
	std::vector<uint32_t> ClosestHitIDs;
	std::vector<uint32_t> AnyHitIDs;

	UniquePtr<UHRenderBuffer<UHRTIndirectLightConstants>> RTIndirectLightConstants[GMaxFrameInFlight];
};

// shader to reprojection RTIL
enum class UHRTIndirectReprojectType
{
	SkyReprojection,
	DiffuseReprojection
};

struct UHRTIndirectReprojectionConstants
{
	uint32_t Resolution[2];
	float AlphaMin;
	float AlphaMax;
};

class UHRTIndirectReprojectionShader : public UHShaderClass
{
public:
	UHRTIndirectReprojectionShader(UHGraphic* InGfx, std::string Name, UHRTIndirectReprojectType InType);
	virtual void OnCompile() override;

	void BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const int32_t FrameIdx);

private:
	UHRTIndirectReprojectType ReprojectionType;
};