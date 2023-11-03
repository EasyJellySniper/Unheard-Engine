#pragma once
#include "ShaderClass.h"

class UHMeshRendererComponent;
class UHDepthPassShader : public UHShaderClass
{
public:
	UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);

	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);
};