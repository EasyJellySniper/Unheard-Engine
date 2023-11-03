#include "PanoramaToCubemapShader.h"

#if WITH_EDITOR
UHPanoramaToCubemapShader::UHPanoramaToCubemapShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHPanoramaToCubemapShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	OnCompile();
}

void UHPanoramaToCubemapShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("PanoramaToCubemapVS", "Shaders/PanoramaToCubemap.hlsl", "PanoramaToCubemapVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("PanoramaToCubemapPS", "Shaders/PanoramaToCubemap.hlsl", "PanoramaToCubemapPS", "ps_6_0");

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