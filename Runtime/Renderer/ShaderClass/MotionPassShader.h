#pragma once
#include "ShaderClass.h"

// shader implementation for camera motion
class UHMotionCameraPassShader : public UHShaderClass
{
public:
	UHMotionCameraPassShader() {}
	UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* DepthTexture
		, const UHSampler* PointClamppedSampler);
};

// shader implementation for object motion
class UHMotionObjectPassShader : public UHShaderClass
{
public:
	UHMotionObjectPassShader() {}
	UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
		, const std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight>& OcclusionConst
		, const UHMeshRendererComponent* InRenderer);
};