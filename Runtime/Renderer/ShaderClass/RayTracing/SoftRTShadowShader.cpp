#include "SoftRTShadowShader.h"

UHSoftRTShadowShader::UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHSoftRTShadowShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();

	ShaderCS = InGfx->RequestShader("SoftRTShadowComputeShader", "Shaders/RayTracing/SoftRTShadowComputeShader.hlsl", "SoftRTShadowCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHSoftRTShadowShader::BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* RTDirShadowResult
	, const UHRenderTexture* RTPointShadowResult
	, const UHRenderTexture* InputRTShadow
	, const UHRenderTexture* DepthTexture
	, const UHRenderTexture* TranslucentDepth
	, const UHRenderTexture* SceneMip
	, const UHSampler* LinearClamppedSampler)
{
	BindConstant(SysConst, 0);
	BindRWImage(RTDirShadowResult, 1);
	BindRWImage(RTPointShadowResult, 2);
	BindImage(InputRTShadow, 3);
	BindImage(DepthTexture, 4);
	BindImage(TranslucentDepth, 5);
	BindImage(SceneMip, 6);
	BindSampler(LinearClamppedSampler, 7);
}