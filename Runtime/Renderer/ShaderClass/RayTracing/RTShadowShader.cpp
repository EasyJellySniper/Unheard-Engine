#include "RTShadowShader.h"

UHRTShadowShader::UHRTShadowShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, UHShader* InAnyHit, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTShadowShader))
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

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShader = InClosestHit;
	RTInfo.AnyHitShader = InAnyHit;
	RTInfo.PayloadSize = sizeof(UHDefaultPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = InGfx->RequestRTState(RTInfo);

	// get shader group data for setting up shader table
	std::vector<BYTE> TempData(InGfx->GetShaderRecordSize());

	PFN_vkGetRayTracingShaderGroupHandlesKHR GetRTShaderGroupHandle = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
		vkGetInstanceProcAddr(InGfx->GetInstance(), "vkGetRayTracingShaderGroupHandlesKHR");
	if (GetRTShaderGroupHandle(InGfx->GetLogicalDevice(), RTState->GetRTPipeline(), 0, 1, InGfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get shader group handle!\n");
	}

	RayGenTable = InGfx->RequestRenderBuffer<UHShaderRecord>();
	RayGenTable->CreaetBuffer(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	RayGenTable->UploadAllData(TempData.data());

	// get data for HG as well
	if (GetRTShaderGroupHandle(InGfx->GetLogicalDevice(), RTState->GetRTPipeline(), 1, 1, InGfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get shader group handle!\n");
	}

	// copy HG records to the buffer, both closest hit and any hit will be put in the same hit group
	HitGroupTable = InGfx->RequestRenderBuffer<UHShaderRecord>();
	HitGroupTable->CreaetBuffer(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	HitGroupTable->UploadAllData(TempData.data());
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