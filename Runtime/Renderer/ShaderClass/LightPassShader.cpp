#include "LightPassShader.h"

UHLightPassShader::UHLightPassShader(UHGraphic* InGfx, std::string Name, int32_t RTInstanceCount)
	: UHShaderClass(InGfx, Name, typeid(UHLightPassShader), nullptr)
{
	// Lighting pass: bind system, light buffer, GBuffers, and samplers, all fragment only since it's a full screen quad draw
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// if multiple descriptor count is used here, I can declare things like: Texture2D GBuffers[4]; in the shader
	// which acts like a "descriptor array"
	AddLayoutBinding(GNumOfGBuffers, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	CreateDescriptor();

	std::vector<std::string> Defines;
	if (RTInstanceCount > 0)
	{
		Defines.push_back("WITH_RTSHADOWS");
	}
	ShaderCS = InGfx->RequestShader("LightComputeShader", "Shaders/LightComputeShader.hlsl", "LightCS", "cs_6_0", Defines);

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHLightPassShader::BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& LightConst
	, const std::vector<UHTexture*>& GBuffers
	, const UHRenderTexture* SceneResult
	, const UHSampler* LinearClamppedSampler
	, const int32_t RTInstanceCount
	, const UHRenderTexture* RTShadowResult)
{
	BindConstant(SysConst, 0);
	BindStorage(LightConst, 1, 0, true);

	BindImage(GBuffers, 2);
	BindImage(SceneResult, 3, -1, true);
	BindSampler(LinearClamppedSampler, 4);

	if (Gfx->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		BindImage(RTShadowResult, 5);
	}
}