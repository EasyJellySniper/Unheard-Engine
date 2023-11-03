#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
class UHDebugViewShader : public UHShaderClass
{
public:
	UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;
};
#endif