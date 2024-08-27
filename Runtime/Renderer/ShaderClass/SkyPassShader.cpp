#include "SkyPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHSkyPassShader::UHSkyPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHSkyPassShader), nullptr, InRenderPass)
{
	// Sky pass: bind system/object layout and sky cube texture/sampler layout
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHSkyPassShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("SkyboxVertexShader", "Shaders/SkyboxVertexShader.hlsl", "SkyboxVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("SkyboxPixelShader", "Shaders/SkyboxPixelShader.hlsl", "SkyboxPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(RenderPassCache, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
		, UHCullMode::CullFront
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateGraphicState(Info);
}

void UHSkyPassShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindSkyCube();
	BindSampler(GSkyCubeSampler, 2);
}

void UHSkyPassShader::BindSkyCube()
{
	BindImage(GSkyLightCube, 1);
}