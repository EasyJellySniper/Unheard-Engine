#include "SkyLight.h"
#include <string>

UHSkyLightComponent::UHSkyLightComponent()
	: AmbientSkyColor(XMFLOAT3())
	, AmbientGroundColor(XMFLOAT3())
	, SkyIntensity(1.0f)
	, GroundIntensity(1.0f)
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
}
#endif