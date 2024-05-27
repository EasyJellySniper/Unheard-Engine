#pragma once
#include "ShaderClass.h"

class UHTranslucentPassShader : public UHShaderClass
{
public:
	UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts
		, bool bInOcclusionTest = false);
	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer, const bool bIsRaytracingEnableRT);
	void BindSkyCube();

	void RecreateOcclusionState();
	UHGraphicState* GetOcclusionState() const;

private:
	bool bOcclusionTest;
	static UHGraphicState* OcclusionState;
};