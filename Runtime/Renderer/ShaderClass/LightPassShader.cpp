#include "LightPassShader.h"

UHLightPassShader::UHLightPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, int32_t RTInstanceCount)
	: UHShaderClass(InGfx, Name, typeid(UHLightPassShader))
{
	// Lighting pass: bind system, light buffer, GBuffers, and samplers, all fragment only since it's a full screen quad draw
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// if multiple descriptor count is used here, I can declare things like: Texture2D GBuffers[4]; in the shader
	// which acts like a "descriptor array"
	AddLayoutBinding(GNumOfGBuffers, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	CreateDescriptor();

	std::vector<std::string> Defines;
	if (RTInstanceCount > 0)
	{
		Defines.push_back("WITH_RTSHADOWS");
	}
	ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = InGfx->RequestShader("LightPixelShader", "Shaders/LightPixelShader.hlsl", "LightPS", "ps_6_0", Defines);

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, VK_CULL_MODE_NONE
		, UHBlendMode::Addition
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	GraphicState = InGfx->RequestGraphicState(Info);
}

void UHLightPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& LightConst
	, const std::vector<UHTexture*>& GBuffers
	, const UHSampler* LinearClamppedSampler
	, const int32_t RTInstanceCount
	, const UHRenderTexture* RTShadowResult)
{
	BindConstant(SysConst, 0);
	BindStorage(LightConst, 1, 0, true);

	BindImage(GBuffers, 2);
	BindSampler(LinearClamppedSampler, 3);

	if (Gfx->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		BindImage(RTShadowResult, 4);
	}
}