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
	UH_SYNC_DETAIL_VALUE("AmbientSky", AmbientSkyColor)
}

void UHSkyLightComponent::SetGroundColor(XMFLOAT3 InColor)
{
	AmbientGroundColor = InColor;
	UH_SYNC_DETAIL_VALUE("AmbientGround", AmbientGroundColor)
}

void UHSkyLightComponent::SetSkyIntensity(float InIntensity)
{
	SkyIntensity = InIntensity;
	UH_SYNC_DETAIL_VALUE("SkyIntensity", SkyIntensity)
}

void UHSkyLightComponent::SetGroundIntensity(float InIntensity)
{
	GroundIntensity = InIntensity;
	UH_SYNC_DETAIL_VALUE("GroundIntensity", GroundIntensity)
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
void UHSkyLightComponent::OnPropertyChange(std::string PropertyName)
{
	UHTransformComponent::OnPropertyChange(PropertyName);

	const UHFloat3DetailGUI* Float3GUI = static_cast<const UHFloat3DetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHFloatDetailGUI* FloatGUI = static_cast<const UHFloatDetailGUI*>(DetailView->GetGUI(PropertyName));

	if (PropertyName == "AmbientSky")
	{
		AmbientSkyColor = Float3GUI->GetValue();
	}
	else if (PropertyName == "AmbientGround")
	{
		AmbientGroundColor = Float3GUI->GetValue();
	}
	else if (PropertyName == "SkyIntensity")
	{
		SkyIntensity = FloatGUI->GetValue();
	}
	else if (PropertyName == "GroundIntensity")
	{
		GroundIntensity = FloatGUI->GetValue();
	}
}

void UHSkyLightComponent::OnGenerateDetailView(HWND ParentWnd)
{
	UHTransformComponent::OnGenerateDetailView(ParentWnd);

	DetailView = MakeUnique<UHDetailView>("Sky Light");
	DetailView->OnGenerateDetailView<UHSkyLightComponent>(this, ParentWnd, DetailStartHeight);
	UH_SYNC_DETAIL_VALUE("AmbientSky", AmbientSkyColor)
	UH_SYNC_DETAIL_VALUE("AmbientGround", AmbientGroundColor)
	UH_SYNC_DETAIL_VALUE("SkyIntensity", SkyIntensity)
	UH_SYNC_DETAIL_VALUE("GroundIntensity", GroundIntensity)
}
#endif