#include "GaussianFilterShader.h"
#include "../../RendererShared.h"

UHGaussianFilterShader::UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType)
	: UHShaderClass(InGfx, Name, typeid(UHGaussianFilterShader), nullptr)
	, GaussianFilterType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	CreateDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		GaussianConstantsGPU[Idx] = InGfx->RequestRenderBuffer<UHGaussianFilterConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void UHGaussianFilterShader::Release(bool bDescriptorOnly)
{
	UHShaderClass::Release(bDescriptorOnly);
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(GaussianConstantsGPU[Idx]);
	}
}

void UHGaussianFilterShader::OnCompile()
{
	std::vector<std::string> Defines;
	Defines.push_back("MAX_FILTER_RADIUS=" + std::to_string(GMaxGaussianFilterRadius));

	if (GaussianFilterType == FilterHorizontal)
	{
		ShaderCS = Gfx->RequestShader("GaussianFilterHShader", "Shaders/PostProcessing/GaussianFilterComputeShader.hlsl", "HorizontalFilterCS", "cs_6_0", Defines);
	}
	else if (GaussianFilterType == FilterVertical)
	{
		ShaderCS = Gfx->RequestShader("GaussianFilterVShader", "Shaders/PostProcessing/GaussianFilterComputeShader.hlsl", "VerticalFilterCS", "cs_6_0", Defines);
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHGaussianFilterShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GaussianConstantsGPU, 1);

	// Bind the I/O textures based on filter direction
	if (GaussianFilterType == FilterHorizontal)
	{
		BindRWImage(GGaussianFilterTempRT0, 2);
		BindImage(GGaussianFilterTempRT1, 3);
	}
	else
	{
		BindRWImage(GGaussianFilterTempRT1, 2);
		BindImage(GGaussianFilterTempRT0, 3);
	}
}

void UHGaussianFilterShader::SetGaussianConstants(UHGaussianFilterConstants InConstants, const int32_t FrameIdx)
{
	GaussianConstantsGPU[FrameIdx]->UploadAllData(&InConstants);
}