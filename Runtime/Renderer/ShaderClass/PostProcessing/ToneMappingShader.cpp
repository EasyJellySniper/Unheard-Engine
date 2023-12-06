#include "ToneMappingShader.h"
#include "../../RendererShared.h"

UHToneMappingShader::UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHToneMappingShader), nullptr, InRenderPass)
{
	// one texture and one sampler for tone mapping
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		ToneMapData[Idx] = Gfx->RequestRenderBuffer<uint32_t>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void UHToneMappingShader::Release(bool bDescriptorOnly)
{
	UHShaderClass::Release(bDescriptorOnly);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(ToneMapData[Idx]);
	}
}

void UHToneMappingShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("ToneMapPixelShader", "Shaders/PostProcessing/ToneMapPixelShader.hlsl", "ToneMapPS", "ps_6_0");

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

void UHToneMappingShader::BindParameters()
{
	BindSampler(GPointClampedSampler, 1);
	BindConstant(ToneMapData, 2);
}

UHRenderBuffer<uint32_t>* UHToneMappingShader::GetToneMapData(int32_t FrameIdx) const
{
	return ToneMapData[FrameIdx].get();
}