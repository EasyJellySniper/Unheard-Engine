#include "RTOcclusionTestShader.h"

UHRTOcclusionTestShader::UHRTOcclusionTestShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, UHShader* InAnyHit, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTOcclusionTestShader), nullptr)
{
	// Occlusion Test: system const + TLAS + result output + material slot
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GMaterialSlotInRT);

	CreateDescriptor(ExtraLayouts);
	RayGenShader = InGfx->RequestShader("RTOcclusionTestShader", "Shaders/RayTracing/RayTracingOcclusionTest.hlsl", "RTOcclusionTestRayGen", "lib_6_3");
	MissShader = InGfx->RequestShader("RTOcclusionTestShader", "Shaders/RayTracing/RayTracingOcclusionTest.hlsl", "RTOcclusionTestMiss", "lib_6_3");

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

void UHRTOcclusionTestShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst)
{
	BindConstant(SysConst, 0);
	BindStorage(OcclusionConst.get(), 2, 0, true);
	BindStorage(MatConst, GMaterialSlotInRT, 0, true);
}