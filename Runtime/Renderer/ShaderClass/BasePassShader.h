#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader() {}
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
		, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
		, const std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight>& OcclusionConst
		, const UHMeshRendererComponent* InRenderer
		, const UHAssetManager* InAssetMgr
		, const UHSampler* InDefaultSampler);
};