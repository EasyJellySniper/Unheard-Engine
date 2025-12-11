#include "RTSmoothNormalShader.h"
#include "../../RendererShared.h"

UHRTSmoothSceneNormalShader::UHRTSmoothSceneNormalShader(UHGraphic* InGfx, std::string Name, bool bInIsVertical)
	: UHShaderClass(InGfx, Name, typeid(UHRTSmoothSceneNormalShader))
	, bIsVertical(bInIsVertical)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHRTSmoothSceneNormalShader::OnCompile()
{
	const std::string EntryName = bIsVertical ? "RTSmoothNormalVerticalCS" : "RTSmoothNormalHorizontalCS";
	ShaderCS = Gfx->RequestShader(Name, "Shaders/RayTracing/RTSmoothSceneNormal.hlsl", EntryName, "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHRTSmoothSceneNormalShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSmoothSceneNormal, 1);
	BindImage(GSceneNormal, 2);
	BindSampler(GLinearClampedSampler, 3);
}