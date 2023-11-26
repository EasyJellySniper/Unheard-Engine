#include "DebugBoundShader.h"

#if WITH_EDITOR
#include "../../RendererShared.h"

UHDebugBoundShader::UHDebugBoundShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHDebugBoundShader), nullptr, InRenderPass)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		DebugBoundData[Idx] = Gfx->RequestRenderBuffer<UHDebugBoundConstant>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void UHDebugBoundShader::Release(bool bDescriptorOnly)
{
	UHShaderClass::Release(bDescriptorOnly);
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(DebugBoundData[Idx]);
	}
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

void UHDebugBoundShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(DebugBoundData, 1);
}

UHRenderBuffer<UHDebugBoundConstant>* UHDebugBoundShader::GetDebugBoundData(const int32_t FrameIdx) const
{
	return DebugBoundData[FrameIdx].get();
}

#endif