#include "LightPassShader.h"
#include "../RendererShared.h"

UHLightPassShader::UHLightPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHLightPassShader), nullptr)
{
	// Lighting pass: bind system, light buffer, GBuffers, and samplers, all fragment only since it's a full screen quad draw
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// other textures/buffers/samplers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		SoftShadowConsts[Idx] =
			Gfx->RequestRenderBuffer<UHSoftShadowConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "SoftShadowConstants");
	}
}

void UHLightPassShader::Release()
{
	UHShaderClass::Release();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(SoftShadowConsts[Idx]);
	}
}

void UHLightPassShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("LightComputeShader", "Shaders/LightComputeShader.hlsl", "LightCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHLightPassShader::BindParameters(const bool bIsRaytracingEnableRT)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindConstant(SoftShadowConsts, 1, 0);
	BindRWImage(GSceneResult, 2);
	BindRWImage(GIndirectOcclusionResult, 3);
	BindImage(GetGBuffersSRV(), 4);

	if (bIsRaytracingEnableRT)
	{
		BindImage(GRTDirectLightResult, 5);
		BindImage(GRTDirectHitDistance, 6);
	}
	else
	{
		BindImage(GBlackTexture, 5);
		BindImage(GBlackTexture, 6);
	}

	BindImage(GRTIndirectLighting, 7);
	BindImage(GSceneMixedDepth, 8);
	BindImage(GSceneMip, 9);
	BindImage(GMotionVectorRT, 10);

	BindStorage(GSH9Data.get(), 11, 0, true);
	BindSampler(GPointClampedSampler, 12);
	BindSampler(GLinearClampedSampler, 13);
}

UHRenderBuffer<UHSoftShadowConstants>* UHLightPassShader::GetConstants(const int32_t FrameIdx) const
{
	return SoftShadowConsts[FrameIdx].get();
}