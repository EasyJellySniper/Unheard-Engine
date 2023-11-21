#pragma once
#include "ShaderClass.h"

#if WITH_EDITOR

class UHMeshPreviewShader : public UHShaderClass
{
public:
	UHMeshPreviewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void OnCompile() override;
};

#endif