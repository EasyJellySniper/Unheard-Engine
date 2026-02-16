#include "RTSkyLightShader.h"
#include "../../RendererShared.h"

UHRTSkyLightShader::UHRTSkyLightShader(UHGraphic* InGfx, std::string Name
	, const std::vector<uint32_t>& InClosestHits
	, const std::vector<uint32_t>& InAnyHits
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHRTSkyLightShader), nullptr)
{
	// system
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// TLAS + result
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	CreateLayoutAndDescriptor(ExtraLayouts);

	ClosestHitIDs = InClosestHits;
	AnyHitIDs = InAnyHits;
	OnCompile();

	InitRayGenTable();
	InitMissTable();
	InitHitGroupTable(InAnyHits.size());
}

void UHRTSkyLightShader::OnCompile()
{
	RayGenShader = Gfx->RequestShader("RTSkyLightShader", "Shaders/RayTracing/RayTracingSkyLight.hlsl", "RTSkyLightRayGen", "lib_6_3");
	MissShaders.push_back(Gfx->RequestShader("RTSkyLightShader", "Shaders/RayTracing/RayTracingSkyLight.hlsl", "RTSkyLightMiss", "lib_6_3"));

	UHRayTracingInfo RTInfo{};
	RTInfo.PipelineLayout = PipelineLayout;
	RTInfo.RayGenShader = RayGenShader;
	RTInfo.ClosestHitShaders = ClosestHitIDs;
	RTInfo.AnyHitShaders = AnyHitIDs;
	RTInfo.MissShaders = MissShaders;
	RTInfo.PayloadSize = sizeof(UHMinimalPayload);
	RTInfo.AttributeSize = sizeof(UHDefaultAttribute);
	RTState = Gfx->RequestRTState(RTInfo);
}

void UHRTSkyLightShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindTLAS(GTopLevelAS[Idx].get(), 1, Idx);
	}

	BindRWImage(GRTSkyData, 2);
	BindRWImage(GRTSkyDiscoverAngle, 3);
}