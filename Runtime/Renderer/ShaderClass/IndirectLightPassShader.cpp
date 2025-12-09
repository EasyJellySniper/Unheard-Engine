#include "IndirectLightPassShader.h"
#include "../RendererShared.h"
#include "RayTracing/RTIndirectLightShader.h"

UHIndirectLightPassShader::UHIndirectLightPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHIndirectLightPassShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHIndirectLightPassShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("IndirectLightComputeShader", "Shaders/IndirectLightComputeShader.hlsl", "IndirectLightCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHIndirectLightPassShader::BindParameters(bool bUseRTIndirectLight)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GIndirectLightResult, 1);
	BindImage(GetGBuffersSRV(), 2);

	if (bUseRTIndirectLight)
	{
		BindImage(GRTIndirectLighting, 3);
	}

	BindImage(GSceneMixedDepth, 4);
	BindImage(GMotionVectorRT, 5);
	BindStorage(GSH9Data.get(), 6, 0, true);
	BindSampler(GLinearClampedSampler, 7);
}