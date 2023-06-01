#include "RTShadowShader.h"

UHRTShadowShader::UHRTShadowShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, UHShader* InAnyHit, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTShadowShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_SAMPLER);

	// define material buffer slot to a specific number
	AddLayoutBinding(1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GMaterialSlotInRT);

	CreateDescriptor(ExtraLayouts);
	RayGenShader = InGfx->RequestShader("RTShadowShader", "Shaders/RayTracing/RayTracingShadow.hlsl", "RTShadowRayGen", "lib_6_3");
	MissShader = InGfx->RequestShader("RTShadowShader", "Shaders/RayTracing/RayTracingShadow.hlsl", "RTShadowMiss", "lib_6_3");

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShader = InClosestHit;
	RTInfo.AnyHitShader = InAnyHit;
	RTInfo.MissShader = MissShader;
	RTInfo.PayloadSize = sizeof(UHDefaultPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = InGfx->RequestRTState(RTInfo);

	InitRayGenTable();
	InitMissTable();
	InitHitGroupTable();
}

void UHRTShadowShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* RTShadowResult
	, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
	, const UHRenderTexture* SceneMip
	, const UHRenderTexture* SceneNormal
	, const UHRenderTexture* SceneDepth
	, const UHSampler* PointClampedSampler
	, const UHSampler* LinearClampedSampler
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst)
{
	BindConstant(SysConst, 0);
	BindRWImage(RTShadowResult, 2);
	BindStorage(DirLights, 3, 0, true);
	BindImage(SceneMip, 4);
	BindImage(SceneNormal, 5);
	BindImage(SceneDepth, 6);
	BindSampler(PointClampedSampler, 7);
	BindSampler(LinearClampedSampler, 8);
	BindStorage(MatConst, GMaterialSlotInRT, 0, true);
}