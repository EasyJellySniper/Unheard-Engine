#include "GaussianBlurShader.h"
#include "../../RendererShared.h"

UHGaussianBlurShader::UHGaussianBlurShader(UHGraphic* InGfx, std::string Name, const UHGaussianBlurType InType)
	: UHShaderClass(InGfx, Name, typeid(UHGaussianBlurShader), nullptr)
	, GaussianBlurType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	
	// Push size constant later
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(VkExtent2D);

	CreateDescriptor();
	OnCompile();
}

void UHGaussianBlurShader::Release(bool bDescriptorOnly)
{
	UHShaderClass::Release(bDescriptorOnly);
}

void UHGaussianBlurShader::OnCompile()
{
	if (GaussianBlurType == BlurHorizontal)
	{
		ShaderCS = Gfx->RequestShader("GaussianBlurHShader", "Shaders/PostProcessing/GaussianBlurComputeShader.hlsl", "HorizontalBlurCS", "cs_6_0");
	}
	else if (GaussianBlurType == BlurVertical)
	{
		ShaderCS = Gfx->RequestShader("GaussianBlurVShader", "Shaders/PostProcessing/GaussianBlurComputeShader.hlsl", "VerticalBlurCS", "cs_6_0");
	}

	// state
	UHComputePassInfo CInfo = UHComputePassInfo(PipelineLayout);
	CInfo.CS = ShaderCS;

	CreateComputeState(CInfo);
}

void UHGaussianBlurShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
}