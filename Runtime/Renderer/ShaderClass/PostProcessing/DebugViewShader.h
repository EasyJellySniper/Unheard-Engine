#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
class UHDebugViewShader : public UHShaderClass
{
public:
	UHDebugViewShader() {}
	UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
};
#endif