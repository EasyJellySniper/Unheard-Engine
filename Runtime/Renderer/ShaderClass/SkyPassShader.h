#pragma once
#include "ShaderClass.h"

// Skypass shader implementation
class UHSkyPassShader : public UHShaderClass
{
public:
	UHSkyPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	void BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const UHMeshRendererComponent* SkyRenderer);
};