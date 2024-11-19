#include "DebugViewShader.h"

#if WITH_EDITOR
#include "../../RendererShared.h"

UHDebugViewShader::UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHDebugViewShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();

	DebugViewData = Gfx->RequestRenderBuffer<uint32_t>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "DebugViewData");
}

void UHDebugViewShader::Release()
{
	UHShaderClass::Release();
	UH_SAFE_RELEASE(DebugViewData);
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

void UHDebugViewShader::BindParameters()
{
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindConstant(DebugViewData, 0, Idx, 0);
	}

	// slot 1 will be bound in fly

	BindSampler(GPointClampedSampler, 2);
}

UHRenderBuffer<uint32_t>* UHDebugViewShader::GetDebugViewData() const
{
	return DebugViewData.get();
}

#endif