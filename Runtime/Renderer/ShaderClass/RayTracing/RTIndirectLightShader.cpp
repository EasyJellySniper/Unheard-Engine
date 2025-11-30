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
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// light lists
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

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
	RTInfo.MaxRecursionDepth = MaxIndirectLightRecursion;
	RTState = Gfx->RequestRTState(RTInfo);
}

void UHRTIndirectLightShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindTLAS(GTopLevelAS[Idx].get(), 1, Idx);
	}

	BindRWImage(GRTIndirectLighting, 2);
	BindConstant(RTIndirectLightConstants, 3, 0);
	BindImage(GetGBuffersSRV(), 4);

	// lights
	BindStorage(GDirectionalLightBuffer, 5, 0, true);
	BindStorage(GPointLightBuffer, 6, 0, true);
	BindStorage(GSpotLightBuffer, 7, 0, true);
	BindStorage(GSH9Data.get(), 8, 0, true);

	// textures
	BindImage(GSceneMip, 9);
	BindImage(GSceneMixedDepth, 10);
	BindImage(GSceneData, 11);
	BindImage(GSmoothSceneNormal, 12);
	BindImage(GTranslucentBump, 13);
	BindImage(GTranslucentSmoothness, 14);

	// light lists (indices)
	BindStorage(GInstanceLightsBuffer, 15, 0, true);
	BindStorage(GPointLightListTransBuffer.get(), 16, 0, true);
	BindStorage(GSpotLightListTransBuffer.get(), 17, 0, true);
}

UHRenderBuffer<UHRTIndirectLightConstants>* UHRTIndirectLightShader::GetConstants(const int32_t FrameIdx) const
{
	return RTIndirectLightConstants[FrameIdx].get();
}