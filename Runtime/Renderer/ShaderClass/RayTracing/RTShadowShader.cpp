#include "RTShadowShader.h"

UHRTShadowShader::UHRTShadowShader(UHGraphic* InGfx, std::string Name
	, const std::vector<uint32_t>& InClosestHits
	, const std::vector<uint32_t>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTShadowShader), nullptr)
{
	// system const
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// TLAS + RT shadow result (float4, xy for dir light, zw for point light)
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	// dir light/point light/spot light/point light culling list (opaque + translucent)/spot light culling list (O+T)
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor(ExtraLayouts);
	RayGenShader = InGfx->RequestShader("RTShadowShader", "Shaders/RayTracing/RayTracingShadow.hlsl", "RTShadowRayGen", "lib_6_3");
	MissShader = InGfx->RequestShader("RTShadowShader", "Shaders/RayTracing/RayTracingShadow.hlsl", "RTShadowMiss", "lib_6_3");

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShaders = InClosestHits;
	RTInfo.AnyHitShaders = InAnyHits;
	RTInfo.MissShader = MissShader;
	RTInfo.PayloadSize = sizeof(UHDefaultPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = InGfx->RequestRTState(RTInfo);

	InitRayGenTable();
	InitMissTable();
	InitHitGroupTable(InAnyHits.size());
}

void UHRTShadowShader::BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* RTShadowResult
	, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
	, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLights
	, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLights
	, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
	, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightListTrans
	, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
	, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightListTrans
	, const UHRenderTexture* SceneMip
	, const UHRenderTexture* SceneDepth
	, const UHRenderTexture* TranslucentDepth
	, const UHRenderTexture* VertexNormal
	, const UHRenderTexture* TranslucentVertexNormal
	, const UHSampler* LinearClampedSampler)
{
	BindConstant(SysConst, 0);

	// TLAS will be bound on the fly

	BindRWImage(RTShadowResult, 2);
	BindStorage(DirLights, 3, 0, true);
	BindStorage(PointLights, 4, 0, true);
	BindStorage(SpotLights, 5, 0, true);
	BindStorage(PointLightList.get(), 6, 0, true);
	BindStorage(PointLightListTrans.get(), 7, 0, true);
	BindStorage(SpotLightList.get(), 8, 0, true);
	BindStorage(SpotLightListTrans.get(), 9, 0, true);
	BindImage(SceneMip, 10);
	BindImage(SceneDepth, 11);
	BindImage(TranslucentDepth, 12);
	BindImage(VertexNormal, 13);
	BindImage(TranslucentVertexNormal, 14);
	BindSampler(LinearClampedSampler, 15);
}