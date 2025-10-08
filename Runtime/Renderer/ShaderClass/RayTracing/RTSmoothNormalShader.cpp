#include "RTSmoothNormalShader.h"
#include "../../RendererShared.h"

RTSmoothSceneNormalShader::RTSmoothSceneNormalShader(UHGraphic* InGfx, std::string Name, bool bInIsVertical)
	: UHShaderClass(InGfx, Name, typeid(RTSmoothSceneNormalShader))
	, bIsVertical(bInIsVertical)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void RTSmoothSceneNormalShader::OnCompile()
{
	const std::string EntryName = bIsVertical ? "RTSmoothNormalVerticalCS" : "RTSmoothNormalHorizontalCS";
	ShaderCS = Gfx->RequestShader(Name, "Shaders/RayTracing/RTSmoothSceneNormal.hlsl", EntryName, "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void RTSmoothSceneNormalShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSmoothSceneNormal, 1);
	BindImage(GSceneNormal, 2);
	BindImage(GTranslucentBump, 3);
	BindSampler(GLinearClampedSampler, 4);
}