#include "RTOcclusionTestShader.h"

UHRTOcclusionTestShader::UHRTOcclusionTestShader(UHGraphic* InGfx, std::string Name, UHShader* InClosestHit, const std::vector<UHShader*>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTOcclusionTestShader), nullptr)
{
	// Occlusion Test: system const + TLAS + result output + material slot
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateDescriptor(ExtraLayouts);
	RayGenShader = InGfx->RequestShader("RTOcclusionTestShader", "Shaders/RayTracing/RayTracingOcclusionTest.hlsl", "RTOcclusionTestRayGen", "lib_6_3");
	MissShader = InGfx->RequestShader("RTOcclusionTestShader", "Shaders/RayTracing/RayTracingOcclusionTest.hlsl", "RTOcclusionTestMiss", "lib_6_3");

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShader = InClosestHit;
	RTInfo.AnyHitShaders = InAnyHits;
	RTInfo.MissShader = MissShader;
	RTInfo.PayloadSize = sizeof(UHDefaultPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = InGfx->RequestRTState(RTInfo);

	InitRayGenTable();
	InitMissTable();
	InitHitGroupTable(InAnyHits.size());
}

void UHRTOcclusionTestShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst)
{
	BindConstant(SysConst, 0);
	BindStorage(OcclusionConst.get(), 2, 0, true);
}