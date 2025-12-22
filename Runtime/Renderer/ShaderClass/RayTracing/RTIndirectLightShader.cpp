#include "RTIndirectLightShader.h"
#include "../../RendererShared.h"

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
	AddLayoutBinding(GNumOfIndirectLightFrames, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(GNumOfIndirectLightFrames, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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

	// light lists
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// sampler
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
	MissShader = Gfx->RequestShader("RTIndirectLightShader", "Shaders/RayTracing/RayTracingIndirectLight.hlsl", "RTIndirectLightMiss", "lib_6_3");

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShaders = ClosestHitIDs;
	RTInfo.AnyHitShaders = AnyHitIDs;
	RTInfo.MissShader = MissShader;
	RTInfo.PayloadSize = sizeof(UHDefaultPayload);
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

	std::vector<UHRenderTexture*> RtilTex;
	for (int32_t Idx = 0; Idx < GNumOfIndirectLightFrames; Idx++)
	{
		RtilTex.push_back(GRTIndirectDiffuse[Idx]);
	}
	BindRWImage(RtilTex, 2);

	RtilTex.clear();
	for (int32_t Idx = 0; Idx < GNumOfIndirectLightFrames; Idx++)
	{
		RtilTex.push_back(GRTIndirectOcclusion[Idx]);
	}
	BindRWImage(RtilTex, 3);

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

	// light lists (indices)
	BindStorage(GInstanceLightsBuffer, 13, 0, true);
	BindStorage(GPointLightListBuffer.get(), 14, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 15, 0, true);

	// sampler
	BindSampler(GPointClampedSampler, 16);
	BindSampler(GLinearClampedSampler, 17);
}

UHRenderBuffer<UHRTIndirectLightConstants>* UHRTIndirectLightShader::GetConstants(const int32_t FrameIdx) const
{
	return RTIndirectLightConstants[FrameIdx].get();
}