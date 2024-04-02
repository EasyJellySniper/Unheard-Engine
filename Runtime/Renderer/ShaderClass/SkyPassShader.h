#pragma once
#include "ShaderClass.h"

// Skypass shader implementation
class UHSkyPassShader : public UHShaderClass
{
public:
	UHSkyPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;

	void BindParameters();
	void BindSkyCube();
};