#include "ReflectionPassShader.h"
#include "../RendererShared.h"

UHReflectionPassShader::UHReflectionPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHReflectionPassShader))
{
	// constant + result + GBuffers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// reflection textures
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// samplers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
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

void UHReflectionPassShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindRWImage(GSceneResult, 1);
	BindImage(GetGBuffersSRV(), 2);

	BindSkyCube();
	if (Gfx->IsRayTracingEnabled())
	{
		BindImage(GRTReflectionResult, 4);
	}
	else
	{
		BindImage(GBlackTexture, 4);
	}

	BindSampler(GSkyCubeSampler,5);
	BindSampler(GPointClampedSampler, 6);
	BindSampler(GLinearClampedSampler, 7);
}

void UHReflectionPassShader::BindSkyCube()
{
	BindImage(GSkyLightCube, 3);
}