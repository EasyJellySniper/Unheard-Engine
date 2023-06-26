#include "RTDefaultHitGroupShader.h"

UHRTDefaultHitGroupShader::UHRTDefaultHitGroupShader(UHGraphic* InGfx, std::string Name, const std::vector<UHMaterial*>& Materials)
	: UHShaderClass(InGfx, Name, typeid(UHRTDefaultHitGroupShader), nullptr)
{
	std::vector<std::string> Defines = { "WITH_CLOSEST" };
	ClosestHitShader = InGfx->RequestShader("RTDefaultHitGroup", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultClosestHit", "lib_6_3", Defines);

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

		std::vector<std::string> MatDefines = Materials[Idx]->GetMaterialDefines(PS);
		MatDefines.push_back("WITH_ANYHIT");

		UHShader* AnyHitShader = InGfx->RequestMaterialShader("RTDefaultHitGroup", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultAnyHit", "lib_6_3", Data, MatDefines);
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

	int32_t Index = UHUtilities::FindIndex(AllMaterials, InMat);

	UHMaterialCompileData Data{};
	Data.bIsHitGroup = true;
	Data.MaterialCache = InMat;

	std::vector<std::string> MatDefines = InMat->GetMaterialDefines(PS);
	MatDefines.push_back("WITH_ANYHIT");

	InGfx->RequestReleaseShader(AnyHitShaders[Index]);
	AnyHitShaders[Index] = InGfx->RequestMaterialShader("RTDefaultHitGroup", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultAnyHit", "lib_6_3", Data, MatDefines);
}