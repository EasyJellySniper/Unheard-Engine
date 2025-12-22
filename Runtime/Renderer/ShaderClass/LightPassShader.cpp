#include "LightPassShader.h"
#include "../RendererShared.h"

UHLightPassShader::UHLightPassShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHLightPassShader), nullptr)
{
	// Lighting pass: bind system, light buffer, GBuffers, and samplers, all fragment only since it's a full screen quad draw
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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
	AddLayoutBinding(GNumOfIndirectLightFrames, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(GNumOfIndirectLightFrames, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
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

void UHLightPassShader::BindParameters(const bool bIsRaytracingEnableRT)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GSceneResult, 1);
	BindRWImage(GRealtimeAOResult, 2);
	BindImage(GetGBuffersSRV(), 3);

	BindStorage(GDirectionalLightBuffer, 4, 0, true);
	BindStorage(GPointLightBuffer, 5, 0, true);
	BindStorage(GSpotLightBuffer, 6, 0, true);
	BindStorage(GPointLightListBuffer.get(), 7, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 8, 0, true);
	BindStorage(GSH9Data.get(), 9, 0, true);

	if (bIsRaytracingEnableRT)
	{
		BindImage(GRTSoftShadow, 10);
		BindImage(GRTReceiveLightBits, 11);
	}
	else
	{
		BindImage(GWhiteTextureArray, 10);
		BindImage(GMaxUIntTexture, 11);
	}

	std::vector<UHTexture*> RtilTex;
	for (int32_t Idx = 0; Idx < GNumOfIndirectLightFrames; Idx++)
	{
		RtilTex.push_back(GRTIndirectDiffuse[Idx]);
	}
	BindImage(RtilTex, 12);

	RtilTex.clear();
	for (int32_t Idx = 0; Idx < GNumOfIndirectLightFrames; Idx++)
	{
		RtilTex.push_back(GRTIndirectOcclusion[Idx]);
	}
	BindImage(RtilTex, 13);

	BindImage(GSceneMixedDepth, 14);
	BindImage(GSceneMip, 15);
	BindImage(GMotionVectorRT, 16);

	BindSampler(GPointClampedSampler, 17);
	BindSampler(GLinearClampedSampler, 18);
}