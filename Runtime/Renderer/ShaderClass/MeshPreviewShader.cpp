#include "MeshPreviewShader.h"

#if WITH_EDITOR

UHMeshPreviewShader::UHMeshPreviewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHMeshPreviewShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHMeshPreviewShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("MeshPreviewVS", "Shaders/MeshPreviewShader.hlsl", "MeshPreviewVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("MeshPreviewPS", "Shaders/MeshPreviewShader.hlsl", "MeshPreviewPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(RenderPassCache, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, UHCullMode::CullNone
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);
	Info.bDrawWireFrame = true;

	CreateGraphicState(Info);
}

#endif