#include "TemporalAAShader.h"

UHTemporalAAShader::UHTemporalAAShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHTemporalAAShader), nullptr)
{
	// a system buffer + one texture and one sampler for TAA
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	
	CreateDescriptor();
	ShaderCS = InGfx->RequestShader("TemporalAACSShader", "Shaders/PostProcessing/TemporalAAComputeShader.hlsl", "TemporalAACS", "cs_6_0");

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHTemporalAAShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* PreviousSceneResult
	, const UHRenderTexture* MotionVectorRT
	, const UHRenderTexture* PrevMotionVectorRT
	, const UHRenderTexture* SceneDepth
	, const UHSampler* LinearClampedSampler)
{
	BindConstant(SysConst, 0);

	// Due to the post processing alternatively reuses temporary RT
	// the scene result is bound in PostProcessing Rendering instead
	// BindImage(OutputRT, 1);
	// BindImage(SceneResult, 2);

	BindImage(PreviousSceneResult, 3);
	BindImage(MotionVectorRT, 4);
	BindImage(PrevMotionVectorRT, 5);
	BindImage(SceneDepth, 6);
	BindSampler(LinearClampedSampler, 7);
}