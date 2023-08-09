#pragma once
#include "ShaderClass.h"

// shader implementation for camera motion
class UHMotionCameraPassShader : public UHShaderClass
{
public:
	UHMotionCameraPassShader() {}
	UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* DepthTexture
		, const UHSampler* PointClamppedSampler);
};

// shader implementation for object motion
class UHMotionObjectPassShader : public UHShaderClass
{
public:
	UHMotionObjectPassShader() {}
	UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const UHMeshRendererComponent* InRenderer);
};