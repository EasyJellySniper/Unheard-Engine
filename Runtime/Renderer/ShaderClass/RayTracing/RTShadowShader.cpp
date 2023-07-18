#include "RTShadowShader.h"

UHRTShadowShader::UHRTShadowShader(UHGraphic* InGfx, std::string Name
	, const std::vector<uint32_t>& InClosestHits
	, const std::vector<uint32_t>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTShadowShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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

void UHRTShadowShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* RTShadowResult
	, const UHRenderTexture* RTTranslucentShadow
	, const std::array<std::unique_ptr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLights
	, const UHRenderTexture* SceneMip
	, const UHRenderTexture* SceneDepth
	, const UHRenderTexture* TranslucentDepth
	, const UHRenderTexture* VertexNormal
	, const UHRenderTexture* TranslucentVertexNormal
	, const UHSampler* LinearClampedSampler)
{
	BindConstant(SysConst, 0);
	BindRWImage(RTShadowResult, 2);
	BindRWImage(RTTranslucentShadow, 3);
	BindStorage(DirLights, 4, 0, true);
	BindImage(SceneMip, 5);
	BindImage(SceneDepth, 6);
	BindImage(TranslucentDepth, 7);
	BindImage(VertexNormal, 8);
	BindImage(TranslucentVertexNormal, 9);
	BindSampler(LinearClampedSampler, 10);
}