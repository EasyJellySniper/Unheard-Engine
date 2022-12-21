#pragma once
#include "ShaderClass.h"

// Skypass shader implementation
class UHSkyPassShader : public UHShaderClass
{
public:
	UHSkyPassShader() {}
	UHSkyPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const UHMeshRendererComponent* SkyRenderer);
};