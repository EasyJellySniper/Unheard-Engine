#include "UpsampleShader.h"
#include "../../RenderBuilder.h"

UHUpsampleShader::UHUpsampleShader(UHGraphic* InGfx, std::string Name, const UHUpsampleMethod InMethod)
	: UHShaderClass(InGfx, Name, typeid(UHUpsampleShader))
	, UpsampleMethod(InMethod)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(UHUpsampleConstants);
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHUpsampleShader::OnCompile()
{
	if (UpsampleMethod == UHUpsampleMethod::Nearest2x2)
	{
		ShaderCS = Gfx->RequestShader("UpsampleShader", "Shaders/PostProcessing/UpsampleComputeShader.hlsl", "UpsampleNearest2x2CS", "cs_6_0");
	}
	else if (UpsampleMethod == UHUpsampleMethod::NearestHorizontal)
	{
		ShaderCS = Gfx->RequestShader("UpsampleShader", "Shaders/PostProcessing/UpsampleComputeShader.hlsl", "UpsampleNearestHorizontalCS", "cs_6_0");
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHUpsampleShader::BindParameters(UHRenderBuilder& RenderBuilder, const int32_t CurrentFrame, UHTexture* Target, UHUpsampleConstants Consts)
{
	RenderBuilder.PushConstant(PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(UHUpsampleConstants), &Consts);
	PushImage(Target, 0, true, 0);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}