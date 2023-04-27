#include "SkyPassShader.h"
#include "../../Components/MeshRenderer.h"

UHSkyPassShader::UHSkyPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHSkyPassShader), nullptr)
{
	// Sky pass: bind system/object layout and sky cube texture/sampler layout
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	ShaderVS = InGfx->RequestShader("SkyboxVertexShader", "Shaders/SkyboxVertexShader.hlsl", "SkyboxVS", "vs_6_0");
	ShaderPS = InGfx->RequestShader("SkyboxPixelShader", "Shaders/SkyboxPixelShader.hlsl", "SkyboxPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
		, VK_CULL_MODE_FRONT_BIT
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateGraphicState(Info);
}

void UHSkyPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const UHMeshRendererComponent* SkyRenderer)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, SkyRenderer->GetBufferDataIndex());
	BindImage(SkyRenderer->GetMaterial()->GetTex(UHMaterialTextureType::SkyCube), 2);
	BindSampler(SkyRenderer->GetMaterial()->GetSampler(UHMaterialTextureType::SkyCube), 3);
}