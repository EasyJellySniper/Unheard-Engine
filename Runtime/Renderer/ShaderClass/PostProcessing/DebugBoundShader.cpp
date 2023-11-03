#include "DebugBoundShader.h"

#if WITH_EDITOR
UHDebugBoundShader::UHDebugBoundShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHDebugBoundShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateDescriptor();
	OnCompile();
}

void UHDebugBoundShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("DebugBoundVS", "Shaders/PostProcessing/DebugBoundShader.hlsl", "DebugBoundVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("DebugBoundPS", "Shaders/PostProcessing/DebugBoundShader.hlsl", "DebugBoundPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(RenderPassCache, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, UHCullMode::CullNone
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);
	Info.bDrawLine = true;

	CreateGraphicState(Info);
}

#endif