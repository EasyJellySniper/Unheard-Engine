#include "LightPassShader.h"
#include "../RendererShared.h"

UHLightPassShader::UHLightPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHLightPassShader), nullptr)
{
	// Lighting pass: bind system, light buffer, GBuffers, and samplers, all fragment only since it's a full screen quad draw
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// light buffers + SH9
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// other textures/buffers/samplers
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHLightPassShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("LightComputeShader", "Shaders/LightComputeShader.hlsl", "LightCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHLightPassShader::BindParameters(const bool bIsRaytracingEnableRT, const bool bEnableRTIndirectLight)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSceneResult, 1);
	BindImage(GetGBuffersSRV(), 2);

	BindStorage(GDirectionalLightBuffer, 3, 0, true);
	BindStorage(GPointLightBuffer, 4, 0, true);
	BindStorage(GSpotLightBuffer, 5, 0, true);
	BindStorage(GPointLightListBuffer.get(), 6, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 7, 0, true);
	BindStorage(GSH9Data.get(), 8, 0, true);

	if (bIsRaytracingEnableRT)
	{
		BindImage(GRTSoftShadow, 9);
		BindImage(GRTReceiveLightBits, 10);
	}
	else
	{
		BindImage(GWhiteTextureArray, 9);
		BindImage(GMaxUIntTexture, 10);
	}

	if (bEnableRTIndirectLight)
	{
		BindImage(GRTIndirectDiffuse, 11);
		BindImage(GRTIndirectOcclusion, 12);
	}
	else
	{
		BindImage(GBlackTexture, 11);
		BindImage(GWhiteTexture, 12);
	}

	BindSampler(GPointClampedSampler, 13);
	BindSampler(GLinearClampedSampler, 14);
}