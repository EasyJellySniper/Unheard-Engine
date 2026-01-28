#include "RTMinimalHitGroupShader.h"
#include "Runtime/Components/MeshRenderer.h"

UHRTMinimalHitGroupShader::UHRTMinimalHitGroupShader(UHGraphic* InGfx, std::string Name, const std::vector<UHMaterial*>& Materials)
	: UHShaderClass(InGfx, Name, typeid(UHRTMinimalHitGroupShader), nullptr)
{
	// push all any hit shaders for material, following the pixel shader defines
	for (size_t Idx = 0; Idx < Materials.size(); Idx++)
	{
		UHMaterialCompileData Data{};
		Data.bIsHitGroup = true;
		Data.MaterialCache = Materials[Idx];

		// closest hit init
		uint32_t ClosestHitShader = InGfx->RequestMaterialShader("RTMinimalClosestHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTMinimalClosestHit", "lib_6_3", Data);
		ClosestHitShaders.push_back(ClosestHitShader);

		// any hit init
		uint32_t AnyHitShader = InGfx->RequestMaterialShader("RTMinimalAnyHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTMinimalAnyHit", "lib_6_3", Data);
		AnyHitShaders.push_back(AnyHitShader);
	}

	AllMaterials = Materials;
}

void UHRTMinimalHitGroupShader::UpdateHitShader(UHGraphic* InGfx, UHMaterial* InMat)
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

	bool bNeedRefreshShaders = false;
	for (const UHObject* Obj : InMat->GetReferenceObjects())
	{
		if (Obj->GetObjectClassId() == UHMeshRendererComponent::ClassId)
		{
			bNeedRefreshShaders = true;
		}
	}

	// only need to refresh shaders if the material is actually used for rendering
	if (!bNeedRefreshShaders)
	{
		return;
	}

	UHMaterialCompileData Data{};
	Data.bIsHitGroup = true;
	Data.MaterialCache = InMat;

	// closest hit update
	InGfx->RequestReleaseShader(ClosestHitShaders[Index]);
	ClosestHitShaders[Index] = InGfx->RequestMaterialShader("RTMinimalClosestHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTMinimalClosestHit", "lib_6_3", Data);

	// any hit update
	InGfx->RequestReleaseShader(AnyHitShaders[Index]);
	AnyHitShaders[Index] = InGfx->RequestMaterialShader("RTMinimalAnyHit", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTMinimalAnyHit", "lib_6_3", Data);
}