#include "Light.h"

// ************************************** Light Base ************************************** //
UHLightBase::UHLightBase()
	: LightColor(XMFLOAT3(1, 1, 1))
	, Intensity(1.0f)
{

}

void UHLightBase::SetLightColor(XMFLOAT3 InLightColor)
{
	LightColor = InLightColor;
	SetRenderDirties(true);
}

void UHLightBase::SetIntensity(float InIntensity)
{
	Intensity = InIntensity;
	SetRenderDirties(true);
}


// ************************************** Directional Light ************************************** //
void UHDirectionalLightComponent::Update()
{
	// light update doesn't need to calculate world matrix
	bTransformChanged = bIsWorldDirty;

	if (IsTransformChanged())
	{
		SetRenderDirties(true);
		bTransformChanged = false;
	}
}

UHDirectionalLightConstants UHDirectionalLightComponent::GetConstants() const
{
	UHDirectionalLightConstants Consts;
	Consts.Color.x = LightColor.x;
	Consts.Color.y = LightColor.y;
	Consts.Color.z = LightColor.z;
	Consts.Dir = Forward;

	Consts.Color.x *= Intensity;
	Consts.Color.y *= Intensity;
	Consts.Color.z *= Intensity;
	Consts.Color.w = Intensity;

	return Consts;
}


// ************************************** Point Light ************************************** //
UHPointLightComponent::UHPointLightComponent()
	: Radius(50.0f)
{

}

void UHPointLightComponent::Update()
{
	// light update doesn't need to calculate world matrix
	bTransformChanged = bIsWorldDirty;

	if (IsTransformChanged())
	{
		SetRenderDirties(true);
		bTransformChanged = false;
	}
}

void UHPointLightComponent::SetRadius(float InRadius)
{
	Radius = InRadius;
}

UHPointLightConstants UHPointLightComponent::GetConstants() const
{
	UHPointLightConstants Consts;
	Consts.Color.x = LightColor.x;
	Consts.Color.y = LightColor.y;
	Consts.Color.z = LightColor.z;
	Consts.Radius = Radius;

	Consts.Color.x *= Intensity;
	Consts.Color.y *= Intensity;
	Consts.Color.z *= Intensity;
	Consts.Color.w = Intensity;

	Consts.Position = Position;

	return Consts;
}