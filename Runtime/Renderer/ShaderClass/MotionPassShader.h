#pragma once
#include "ShaderClass.h"

// shader implementation for camera motion
class UHMotionCameraPassShader : public UHShaderClass
{
public:
	UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;

	void BindParameters();
};

// shader implementation for object motion
class UHMotionObjectPassShader : public UHShaderClass
{
public:
	UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);
	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);

private:
	bool bHasDepthPrePass;
};