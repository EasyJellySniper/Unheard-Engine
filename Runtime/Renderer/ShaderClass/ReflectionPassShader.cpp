#include "ReflectionPassShader.h"
#include "../RendererShared.h"

UHReflectionPassShader::UHReflectionPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHReflectionPassShader))
{
	// constant + result + GBuffers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// textures
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// samplers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHReflectionPassShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("ReflectionComputeShader", "Shaders/ReflectionComputeShader.hlsl", "ReflectionCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHReflectionPassShader::BindParameters(const bool bIsRaytracingEnableRT)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSceneResult, 1);
	BindImage(GetGBuffersSRV(), 2);

	BindSkyCube();
	if (bIsRaytracingEnableRT)
	{
		BindImage(GRTReflectionResult, 4);
		BindImage(GIndirectLightResult, 5);
	}
	else
	{
		BindImage(GBlackTexture, 4);
		BindImage(GBlackTexture, 5);
	}

	BindSampler(GSkyCubeSampler,6);
	BindSampler(GPointClampedSampler, 7);
	BindSampler(GLinearClampedSampler, 8);
}

void UHReflectionPassShader::BindSkyCube()
{
	BindImage(GSkyLightCube, 3);
}