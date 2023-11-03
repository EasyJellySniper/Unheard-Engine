#pragma once
#include "../ShaderClass.h"

class UHToneMappingShader : public UHShaderClass
{
public:
	UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;
};