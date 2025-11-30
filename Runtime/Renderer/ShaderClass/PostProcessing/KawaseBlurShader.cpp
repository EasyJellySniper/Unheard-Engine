#include "KawaseBlurShader.h"
#include "../../RenderBuilder.h"
#include "../../RendererShared.h"

UHKawaseBlurShader::UHKawaseBlurShader(UHGraphic* InGfx, std::string Name, UHKawaseBlurType InType)
	: UHShaderClass(InGfx, Name, typeid(UHKawaseBlurShader), nullptr)
	, KawaseBlurType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(uint32_t) * 2;
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHKawaseBlurShader::OnCompile()
{
	if (KawaseBlurType == UHKawaseBlurType::Downsample)
	{
		ShaderCS = Gfx->RequestShader("KawaseBlurDownsample", "Shaders/PostProcessing/KawaseBlurComputeShader.hlsl", "KawaseDownsampleCS", "cs_6_0");
	}
	else
	{
		ShaderCS = Gfx->RequestShader("KawaseBlurUpsample", "Shaders/PostProcessing/KawaseBlurComputeShader.hlsl", "KawaseUpsampleCS", "cs_6_0");
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHKawaseBlurShader::BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output)
{
	PushImage(Output, 0, true, 0);
	PushImage(Input, 1, false, 0);
	PushSampler(GLinearClampedSampler, 2);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}