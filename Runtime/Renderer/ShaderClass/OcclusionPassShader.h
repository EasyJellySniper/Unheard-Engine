#pragma once
#include "ShaderClass.h"

// occlusion pass shader, the define is almost the same as depth shader, but does not need a pixel shader
class UHOcclusionPassShader : public UHShaderClass
{
public:
	UHOcclusionPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);

	virtual void OnCompile() override;

	void BindParameters(const UHMeshRendererComponent* InRenderer);

	static UHGraphicState* GetOcclusionState();

private:
	static UHGraphicState* OcclusionStateCache;
};