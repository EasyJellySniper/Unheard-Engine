#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts
		, bool bInOcclusionTest = false);

	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);

	void RecreateOcclusionState();
	UHGraphicState* GetOcclusionState() const;

private:
	bool bOcclusionTest;
	static UHGraphicState* OcclusionState;
};