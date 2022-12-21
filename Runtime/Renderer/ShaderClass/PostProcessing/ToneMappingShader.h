#pragma once
#include "../ShaderClass.h"

class UHToneMappingShader : public UHShaderClass
{
public:
	UHToneMappingShader() {}
	UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
};