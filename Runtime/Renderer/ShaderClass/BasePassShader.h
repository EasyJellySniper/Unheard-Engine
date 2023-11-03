#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass, bool bHasEnvCube
		, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);

private:
	bool bHasDepthPrePass;
	bool bHasEnvCubemap;
};