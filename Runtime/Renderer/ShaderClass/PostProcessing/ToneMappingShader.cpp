#include "ToneMappingShader.h"
#include "../../RendererShared.h"

UHToneMappingShader::UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHToneMappingShader), nullptr, InRenderPass)
{
	// simply system constant + one texture and one sampler for tone mapping
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		ToneMapData[Idx] = InGfx->RequestRenderBuffer<UHToneMapData>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "ToneMapData");
	}
}

void UHToneMappingShader::Release()
{
	UHShaderClass::Release();
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		ToneMapData[Idx]->Release();
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
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindConstant(ToneMapData, 1, 0);
	BindSampler(GPointClampedSampler, 3);
}

void UHToneMappingShader::UploadToneMapData(UHToneMapData InData, const int32_t CurrentFrame)
{
	ToneMapData[CurrentFrame]->UploadData(&InData, 0);
}

void UHToneMappingShader::BindInputImage(UHTexture* InImage, const int32_t CurrentFrameRT)
{
	BindImage(InImage, 2, CurrentFrameRT, false, UHINDEXNONE);
}