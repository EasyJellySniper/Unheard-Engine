#include "TemporalAAShader.h"

UHTemporalAAShader::UHTemporalAAShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHTemporalAAShader))
{
	// a system buffer + one texture and one sampler for TAA
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = InGfx->RequestShader("TemporalAAShader", "Shaders/PostProcessing/TemporalAAPixelShader.hlsl", "TemporalAAPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, VK_CULL_MODE_NONE
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	GraphicState = InGfx->RequestGraphicState(Info);
}

void UHTemporalAAShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* PreviousSceneResult
	, const UHRenderTexture* MotionVectorRT
	, const UHRenderTexture* PrevMotionVectorRT
	, const UHRenderTexture* SceneDepth
	, const UHSampler* LinearClampedSampler)
{
	BindConstant(SysConst, 0);
	BindImage(PreviousSceneResult, 2);
	BindImage(MotionVectorRT, 3);
	BindImage(PrevMotionVectorRT, 4);
	BindImage(SceneDepth, 5);
	BindSampler(LinearClampedSampler, 6);
}