#pragma once
#include "ShaderClass.h"

#if WITH_EDITOR
class UHPanoramaToCubemapShader : public UHShaderClass
{
public:
	UHPanoramaToCubemapShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;
};
#endif