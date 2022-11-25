#pragma once
#include "../ShaderClass.h"

class UHToneMappingShader : public UHShaderClass
{
public:
	UHToneMappingShader() {}
	UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
		: UHShaderClass(InGfx, Name, typeid(UHToneMappingShader))
	{
		// one texture and one sampler for tone mapping
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

		CreateDescriptor();
		ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
		ShaderPS = InGfx->RequestShader("ToneMapPixelShader", "Shaders/PostProcessing/ToneMapPixelShader.hlsl", "ToneMapPS", "ps_6_0");

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
};