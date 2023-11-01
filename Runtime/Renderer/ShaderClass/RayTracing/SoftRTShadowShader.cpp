#include "SoftRTShadowShader.h"
#include "../../RendererShared.h"

UHSoftRTShadowShader::UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHSoftRTShadowShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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

void UHSoftRTShadowShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindRWImage(GRTShadowResult, 1);
	BindImage(GRTSharedTextureRG16F, 2);
	BindImage(GSceneDepth, 3);
	BindImage(GSceneTranslucentDepth, 4);
	BindImage(GSceneMip, 5);
	BindSampler(GLinearClampedSampler, 6);
}