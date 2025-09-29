#include "RTSmoothReflectShader.h"
#include "../../RendererShared.h"

RTSmoothReflectShader::RTSmoothReflectShader(UHGraphic* InGfx, std::string Name, bool bInIsVertical)
	: UHShaderClass(InGfx, Name, typeid(RTSmoothReflectShader))
	, bIsVertical(bInIsVertical)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void RTSmoothReflectShader::OnCompile()
{
	const std::string EntryName = bIsVertical ? "RTSmoothReflectVerticalCS" : "RTSmoothReflectHorizontalCS";
	ShaderCS = Gfx->RequestShader(Name, "Shaders/RayTracing/RTSmoothReflectVector.hlsl", EntryName, "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void RTSmoothReflectShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSmoothSceneNormal, 1);
	BindImage(GSceneNormal, 2);
	BindImage(GTranslucentBump, 3);
	BindImage(GSceneTranslucentDepth, 4);
	BindSampler(GLinearClampedSampler, 5);
}