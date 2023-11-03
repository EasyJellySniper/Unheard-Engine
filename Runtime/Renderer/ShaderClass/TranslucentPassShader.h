#pragma once
#include "ShaderClass.h"

class UHTranslucentPassShader : public UHShaderClass
{
public:
	UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bHasEnvCube, const std::vector<VkDescriptorSetLayout>& ExtraLayouts);
	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);

private:
	bool bHasEnvCubemap;
};