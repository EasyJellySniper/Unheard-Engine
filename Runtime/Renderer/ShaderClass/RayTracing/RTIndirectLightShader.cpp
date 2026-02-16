#include "RTIndirectLightShader.h"
#include "../../RendererShared.h"
#include "../../RenderBuilder.h"

// --------------------------- UHRTIndirectLightShader
UHRTIndirectLightShader::UHRTIndirectLightShader(UHGraphic* InGfx, std::string Name
	, const std::vector<uint32_t>& InClosestHits
	, const std::vector<uint32_t>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTIndirectLightShader), nullptr)
{
	// system
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// TLAS + RT result + shader const
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// GBuffers
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// lights + SH9
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// related textures
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// light lists
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor(ExtraLayouts);

	ClosestHitIDs = InClosestHits;
	AnyHitIDs = InAnyHits;
	OnCompile();

	InitRayGenTable();
	InitMissTable();
	InitHitGroupTable(InAnyHits.size());

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		RTIndirectLightConstants[Idx] = 
			Gfx->RequestRenderBuffer<UHRTIndirectLightConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "ILConstants");
	}
}

void UHRTIndirectLightShader::Release()
{
	UHShaderClass::Release();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(RTIndirectLightConstants[Idx]);
	}
}

void UHRTIndirectLightShader::OnCompile()
{
	RayGenShader = Gfx->RequestShader("RTIndirectLightShader", "Shaders/RayTracing/RayTracingIndirectLight.hlsl", "RTIndirectLightRayGen", "lib_6_3");
	MissShaders.push_back(Gfx->RequestShader("RTIndirectLightShader", "Shaders/RayTracing/RayTracingIndirectLight.hlsl", "RTIndirectShadowMiss", "lib_6_3"));
	MissShaders.push_back(Gfx->RequestShader("RTIndirectLightShader", "Shaders/RayTracing/RayTracingIndirectLight.hlsl", "RTIndirectLightMiss", "lib_6_3"));

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShaders = ClosestHitIDs;
	RTInfo.AnyHitShaders = AnyHitIDs;
	RTInfo.MissShaders = MissShaders;
	RTInfo.PayloadSize = sizeof(UHMinimalPayload) + sizeof(UHIndirectPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = Gfx->RequestRTState(RTInfo);
}

void UHRTIndirectLightShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindTLAS(GTopLevelAS[Idx].get(), 1, Idx);
	}

	BindRWImage(GRTIndirectDiffuse, 2, 0);
	BindRWImage(GRTIndirectOcclusion, 3, 0);
	BindConstant(RTIndirectLightConstants, 4, 0);
	BindImage(GetGBuffersSRV(), 5);

	// lights
	BindStorage(GDirectionalLightBuffer, 6, 0, true);
	BindStorage(GPointLightBuffer, 7, 0, true);
	BindStorage(GSpotLightBuffer, 8, 0, true);
	BindStorage(GSH9Data.get(), 9, 0, true);

	// textures
	BindImage(GSceneMip, 10);
	BindImage(GSceneData, 11);
	BindImage(GSmoothSceneNormal, 12);
	BindImage(GRTSkyData, 13);

	// light lists (indices)
	BindStorage(GInstanceLightsBuffer, 14, 0, true);
	BindStorage(GPointLightListBuffer.get(), 15, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 16, 0, true);

	// sampler
	BindSampler(GPointClampedSampler, 17);
	BindSampler(GLinearClampedSampler, 18);
	BindSampler(GPointClamped3DSampler, 19);
	BindSampler(GLinearClamped3DSampler, 20);
}

UHRenderBuffer<UHRTIndirectLightConstants>* UHRTIndirectLightShader::GetConstants(const int32_t FrameIdx) const
{
	return RTIndirectLightConstants[FrameIdx].get();
}

// --------------------------- UHRTIndirectReprojectionShader
UHRTIndirectReprojectionShader::UHRTIndirectReprojectionShader(UHGraphic* InGfx, std::string Name, UHRTIndirectReprojectType InType)
	: UHShaderClass(InGfx, Name, typeid(UHRTIndirectReprojectionShader), nullptr)
	, ReprojectionType(InType)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// utilize push constants, so the shader could be reused
	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(UHRTIndirectReprojectionConstants);
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHRTIndirectReprojectionShader::OnCompile()
{
	if (ReprojectionType == UHRTIndirectReprojectType::SkyReprojection)
	{
		ShaderCS = Gfx->RequestShader(Name, "Shaders/RayTracing/RTIndirectLightReprojection.hlsl", "RTSkyReprojectionCS", "cs_6_0");
	}
	else
	{
		ShaderCS = Gfx->RequestShader(Name, "Shaders/RayTracing/RTIndirectLightReprojection.hlsl", "RTDiffuseReprojectionCS", "cs_6_0");
	}

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHRTIndirectReprojectionShader::BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const int32_t FrameIdx)
{
	PushConstantBuffer(GSystemConstantBuffer[FrameIdx], 0, 0);
	PushImage(Output, 1, true, 0);

	PushImage(Input, 2, false, 0);
	PushImage(GMotionVectorRT, 3, false, 0);
	PushImage(GSceneDepth, 4, false, 0);
	PushImage(GSceneNormal, 5, false, 0);
	PushImage(GHistoryDepth, 6, false, 0);
	PushImage(GHistoryNormal, 7, false, 0);

	PushSampler(GPointClampedSampler, 8);
	PushSampler(GLinearClampedSampler, 9);

	FlushPushDescriptor(RenderBuilder.GetCmdList());
}