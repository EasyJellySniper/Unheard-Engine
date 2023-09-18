#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
class UHDebugBoundShader : public UHShaderClass
{
public:
	UHDebugBoundShader() {}
	UHDebugBoundShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
};
#endif