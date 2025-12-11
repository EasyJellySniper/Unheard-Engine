#include "RTDirectLightShader.h"
#include "../../RendererShared.h"

UHRTDirectLightShader::UHRTDirectLightShader(UHGraphic* InGfx, std::string Name
	, const std::vector<uint32_t>& InClosestHits
	, const std::vector<uint32_t>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTDirectLightShader), nullptr)
{
	// system const
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// TLAS + RT direct light result
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	// GBuffers
	AddLayoutBinding(GNumOfGBuffersSRV, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// dir light/point light/spot light/point light culling list (opaque)/spot light culling list (opaque)
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// related textures
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

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
}

void UHRTDirectLightShader::OnCompile()
{
	RayGenShader = Gfx->RequestShader("RTDirectLightShader", "Shaders/RayTracing/RayTracingDirectLight.hlsl", "RTDirectLightRayGen", "lib_6_3");
	MissShader = Gfx->RequestShader("RTDirectLightShader", "Shaders/RayTracing/RayTracingDirectLight.hlsl", "RTDirectLightMiss", "lib_6_3");

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

void UHRTDirectLightShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindTLAS(GTopLevelAS[Idx].get(), 1, Idx);
	}

	BindRWImage(GRTDirectLightResult, 2);
	BindRWImage(GRTDirectHitDistance, 3);
	BindImage(GetGBuffersSRV(), 4);

	// light buffers
	BindStorage(GDirectionalLightBuffer, 5, 0, true);
	BindStorage(GPointLightBuffer, 6, 0, true);
	BindStorage(GSpotLightBuffer, 7, 0, true);
	BindStorage(GPointLightListBuffer.get(), 8, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 9, 0, true);

	// translucent buffers and samplers
	BindImage(GSceneMip, 10);
	BindSampler(GPointClampedSampler, 11);
	BindSampler(GLinearClampedSampler, 12);
}