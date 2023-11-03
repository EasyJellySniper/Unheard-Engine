#include "SkyLight.h"
#include <string>
#if WITH_EDITOR
#include "../Engine/Asset.h"
#include "../Renderer/DeferredShadingRenderer.h"
#endif

UHSkyLightComponent::UHSkyLightComponent()
	: AmbientSkyColor(XMFLOAT3())
	, AmbientGroundColor(XMFLOAT3())
	, SkyIntensity(1.0f)
	, GroundIntensity(1.0f)
	, CubemapCache(nullptr)
{
	SetName("SkyLightLightComponent" + std::to_string(GetId()));
}

void UHSkyLightComponent::SetSkyColor(XMFLOAT3 InColor)
{
	AmbientSkyColor = InColor;
}

void UHSkyLightComponent::SetGroundColor(XMFLOAT3 InColor)
{
	AmbientGroundColor = InColor;
}

void UHSkyLightComponent::SetSkyIntensity(float InIntensity)
{
	SkyIntensity = InIntensity;
}

void UHSkyLightComponent::SetGroundIntensity(float InIntensity)
{
	GroundIntensity = InIntensity;
}

XMFLOAT3 UHSkyLightComponent::GetSkyColor() const
{
	return AmbientSkyColor;
}

XMFLOAT3 UHSkyLightComponent::GetGroundColor() const
{
	return AmbientGroundColor;
}

float UHSkyLightComponent::GetSkyIntensity() const
{
	return SkyIntensity;
}

float UHSkyLightComponent::GetGroundIntensity() const
{
	return GroundIntensity;
}

void UHSkyLightComponent::SetCubemap(UHTextureCube* InCube)
{
	CubemapCache = InCube;
}

UHTextureCube* UHSkyLightComponent::GetCubemap() const
{
	return CubemapCache;
}

#if WITH_EDITOR
void UHSkyLightComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();
	ImGui::NewLine();
	ImGui::Text("------ Sky Light ------");

	ImGui::InputFloat3("AmbientSky", (float*)&AmbientSkyColor);
	ImGui::InputFloat3("AmbientGroundColor", (float*)&AmbientGroundColor);
	ImGui::InputFloat("SkyIntensity", &SkyIntensity);
	ImGui::InputFloat("GroundIntensity", &GroundIntensity);

	// texture cube list
	const UHAssetManager* AssetMgr = UHAssetManager::GetAssetMgrEditor();
	if (ImGui::BeginCombo("Cubemap", (CubemapCache) ? CubemapCache->GetName().c_str() : "##"))
	{
		const std::vector<UHTextureCube*>& Cubemaps = AssetMgr->GetCubemaps();
		for (size_t Idx = 0; Idx < Cubemaps.size(); Idx++)
		{
			bool bIsSelected = (CubemapCache == Cubemaps[Idx]);
			if (ImGui::Selectable(Cubemaps[Idx]->GetName().c_str(), bIsSelected))
			{
				bool bNeedRecompile = false;
				if ((CubemapCache == nullptr && Cubemaps[Idx])
					|| (CubemapCache && Cubemaps[Idx] == nullptr))
				{
					bNeedRecompile = true;
				}

				CubemapCache = Cubemaps[Idx];
				UHDeferredShadingRenderer::GetRendererEditorOnly()->RefreshSkyLight(bNeedRecompile);
			}
		}

		ImGui::EndCombo();
	}
}
#endif