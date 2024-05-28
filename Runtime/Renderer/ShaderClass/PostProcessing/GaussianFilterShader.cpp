#include "GaussianFilterShader.h"
#include "../../RendererShared.h"
#include "../../RenderBuilder.h"

UHGaussianFilterShader::UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType)
	: UHShaderClass(InGfx, Name, typeid(UHGaussianFilterShader), nullptr)
	, GaussianFilterType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// utilize push constants, so the shader could be reused
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(UHGaussianFilterConstants);
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

void UHGaussianFilterShader::BindParameters(UHRenderBuilder& RenderBuilder, const int32_t CurrentFrame, UHTexture* Input, UHTexture* Output)
{
	PushConstantBuffer(GSystemConstantBuffer[CurrentFrame], 0);
	PushImage(Output, 1, true);
	PushImage(Input, 2);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}