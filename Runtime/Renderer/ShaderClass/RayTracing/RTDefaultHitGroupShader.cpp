#include "RTDefaultHitGroupShader.h"

UHRTDefaultHitGroupShader::UHRTDefaultHitGroupShader(UHGraphic* InGfx, std::string Name, const std::vector<UHMaterial*>& Materials)
	: UHShaderClass(InGfx, Name, typeid(UHRTDefaultHitGroupShader), nullptr)
{
	// push all any hit shaders for material, following the pixel shader defines
	for (size_t Idx = 0; Idx < Materials.size(); Idx++)
	{
		// skip skybox material
		if (Materials[Idx]->IsSkybox())
		{
			continue;
		}

		UHMaterialCompileData Data{};
		Data.bIsHitGroup = true;
		Data.MaterialCache = Materials[Idx];

		// closest hit init
		std::vector<std::string> MatDefines = Materials[Idx]->GetMaterialDefines();
		uint32_t ClosestHitShader = InGfx->RequestMaterialShader("RTDefaultClosestHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultClosestHit", "lib_6_3", Data, MatDefines);
		ClosestHitShaders.push_back(ClosestHitShader);

		// any hit init
		uint32_t AnyHitShader = InGfx->RequestMaterialShader("RTDefaultAnyHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultAnyHit", "lib_6_3", Data, MatDefines);
		AnyHitShaders.push_back(AnyHitShader);
	}

	AllMaterials = Materials;
}

void UHRTDefaultHitGroupShader::UpdateHitShader(UHGraphic* InGfx, UHMaterial* InMat)
{
	if (InMat == nullptr)
	{
		return;
	}

	const int32_t Index = UHUtilities::FindIndex(AllMaterials, InMat);
	if (Index == UHINDEXNONE)
	{
		return;
	}

	UHMaterialCompileData Data{};
	Data.bIsHitGroup = true;
	Data.MaterialCache = InMat;

	// closest hit update
	std::vector<std::string> MatDefines = InMat->GetMaterialDefines();
	InGfx->RequestReleaseShader(ClosestHitShaders[Index]);
	ClosestHitShaders[Index] = InGfx->RequestMaterialShader("RTDefaultClosestHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultClosestHit", "lib_6_3", Data, MatDefines);

	// any hit update
	InGfx->RequestReleaseShader(AnyHitShaders[Index]);
	AnyHitShaders[Index] = InGfx->RequestMaterialShader("RTDefaultAnyHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultAnyHit", "lib_6_3", Data, MatDefines);
}