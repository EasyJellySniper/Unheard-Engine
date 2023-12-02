#include "TemporalAAShader.h"
#include "../../RendererShared.h"

UHTemporalAAShader::UHTemporalAAShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHTemporalAAShader), nullptr)
{
	// a system buffer + one texture and one sampler for TAA
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	
	CreateDescriptor();
	OnCompile();
}

void UHTemporalAAShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("TemporalAACSShader", "Shaders/PostProcessing/TemporalAAComputeShader.hlsl", "TemporalAACS", "cs_6_0");

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHTemporalAAShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);

	// Due to the post processing alternatively reuses temporary RT
	// the scene result is bound in PostProcessing Rendering instead
	// BindImage(OutputRT, 1);
	// BindImage(SceneResult, 2);

	BindImage(GPreviousSceneResult, 3);
	BindImage(GMotionVectorRT, 4);
	BindSampler(GLinearClampedSampler, 5);
}