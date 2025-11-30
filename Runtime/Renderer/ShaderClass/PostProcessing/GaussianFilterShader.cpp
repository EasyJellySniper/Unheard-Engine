#include "GaussianFilterShader.h"
#include "../../RenderBuilder.h"

UHGaussianFilterShader::UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType)
	: UHShaderClass(InGfx, Name, typeid(UHGaussianFilterShader), nullptr)
	, GaussianFilterType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// utilize push constants, so the shader could be reused
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(float) * 16;
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHGaussianFilterShader::OnCompile()
{
	std::vector<std::string> Defines;
	Defines.push_back("MAX_FILTER_RADIUS=" + std::to_string(GMaxGaussianFilterRadius));

	if (GaussianFilterType == UHGaussianFilterType::FilterHorizontal)
	{
		ShaderCS = Gfx->RequestShader("GaussianFilterHShader", "Shaders/PostProcessing/GaussianFilterComputeShader.hlsl", "HorizontalFilterCS", "cs_6_0", Defines);
	}
	else if (GaussianFilterType == UHGaussianFilterType::FilterVertical)
	{
		ShaderCS = Gfx->RequestShader("GaussianFilterVShader", "Shaders/PostProcessing/GaussianFilterComputeShader.hlsl", "VerticalFilterCS", "cs_6_0", Defines);
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHGaussianFilterShader::BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output)
{
	PushImage(Output, 0, true, 0);
	PushImage(Input, 1, false, 0);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}