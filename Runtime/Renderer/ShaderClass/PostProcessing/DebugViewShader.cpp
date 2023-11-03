#include "DebugViewShader.h"

#if WITH_EDITOR
UHDebugViewShader::UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHDebugViewShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	OnCompile();
}

void UHDebugViewShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("DebugViewPixelShader", "Shaders/PostProcessing/DebugViewPixelShader.hlsl", "DebugViewPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(RenderPassCache, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, UHCullMode::CullNone
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateGraphicState(Info);
}
#endif