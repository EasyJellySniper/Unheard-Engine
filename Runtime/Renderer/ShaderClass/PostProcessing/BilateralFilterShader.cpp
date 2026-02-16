#include "BilateralFilterShader.h"
#include "../../RenderBuilder.h"
#include "../../RendererShared.h"

UHBilateralFilterShader::UHBilateralFilterShader(UHGraphic* InGfx, std::string Name, const UHBilaterialFilterType InType)
	: UHShaderClass(InGfx, Name, typeid(UHBilateralFilterShader), nullptr)
	, FilterType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// utilize push constants, so the shader could be reused
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(float) * 8;
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHBilateralFilterShader::OnCompile()
{
	std::vector<std::string> Defines;
	Defines.push_back("MAX_FILTER_RADIUS=" + std::to_string(GMaxBilateralFilterRadius));

	if (FilterType == UHBilaterialFilterType::FilterHorizontal)
	{
		ShaderCS = Gfx->RequestShader("BilateralFilterHShader", "Shaders/PostProcessing/BilateralFilterComputeShader.hlsl", "HorizontalFilterCS", "cs_6_0", Defines);
	}
	else if (FilterType == UHBilaterialFilterType::FilterVertical)
	{
		ShaderCS = Gfx->RequestShader("BilateralFilterVShader", "Shaders/PostProcessing/BilateralFilterComputeShader.hlsl", "VerticalFilterCS", "cs_6_0", Defines);
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHBilateralFilterShader::BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const int32_t FrameIdx)
{
	PushConstantBuffer(GSystemConstantBuffer[FrameIdx], 0, 0);
	PushImage(Output, 1, true, 0);
	PushImage(Input, 2, false, 0);
	PushImage(GSceneDepth, 3, false, 0);
	PushImage(GSceneNormal, 4, false, 0);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}