#include "DownsampleDepthShader.h"
#include "../RendererShared.h"

UHDownsampleDepthShader::UHDownsampleDepthShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHDownsampleDepthShader))
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	CreateDescriptor();
	OnCompile();
}

void UHDownsampleDepthShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("HalfDownsampleDepthShader", "Shaders/DownsampleDepth.hlsl", "HalfDownsampleDepthCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHDownsampleDepthShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindRWImage(GHalfDepth, 1);
	BindRWImage(GHalfTranslucentDepth, 2);
	BindImage(GSceneDepth, 3);
	BindImage(GSceneTranslucentDepth, 4);
}