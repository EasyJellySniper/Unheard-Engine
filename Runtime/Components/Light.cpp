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
UHDirectionalLightComponent::UHDirectionalLightComponent()
{
	SetName("DirectionalLightComponent" + std::to_string(GetId()));
}

void UHDirectionalLightComponent::Update()
{
	if (!bIsEnabled)
	{
		return;
	}

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
	UHDirectionalLightConstants Consts{};
	if (!bIsEnabled)
	{
		Consts.IsEnabled = 0;
		return Consts;
	}

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

#if WITH_EDITOR
void UHDirectionalLightComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();
	ImGui::NewLine();
	ImGui::Text("------ Directional Light ------");

	ImGui::InputFloat3("LightColor", (float*)&LightColor);
	ImGui::InputFloat("Intensity", &Intensity);
}

void UHDirectionalLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
}

void UHDirectionalLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
}
#endif


// ************************************** Point Light ************************************** //
UHPointLightComponent::UHPointLightComponent()
	: Radius(50.0f)
{
	SetName("PointLightComponent" + std::to_string(GetId()));
}

void UHPointLightComponent::Update()
{
	if (!bIsEnabled)
	{
		return;
	}

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

float UHPointLightComponent::GetRadius() const
{
	return Radius;
}

UHPointLightConstants UHPointLightComponent::GetConstants() const
{
	UHPointLightConstants Consts{};
	if (!bIsEnabled)
	{
		Consts.IsEnabled = 0;
		return Consts;
	}

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

#if WITH_EDITOR
UHDebugBoundConstant UHPointLightComponent::GetDebugBoundConst() const
{
	UHDebugBoundConstant BoundConst{};
	BoundConst.BoundType = DebugSphere;
	BoundConst.Position = GetPosition();
	BoundConst.Radius = GetRadius();
	BoundConst.Color = XMFLOAT3(1, 1, 0);

	return BoundConst;
}

void UHPointLightComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();
	ImGui::NewLine();
	ImGui::Text("------ Point Light ------");

	ImGui::InputFloat3("LightColor", (float*)&LightColor);
	ImGui::InputFloat("Intensity", &Intensity);
	ImGui::InputFloat("Radius", &Radius);
}

void UHPointLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
}

void UHPointLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
}
#endif


// ************************************** Spot Light ************************************** //
UHSpotLightComponent::UHSpotLightComponent()
	: Radius(50.0f)
	, Angle(90.0f)
	, InnerAngle(90.0f * 0.9f)
{
	SetName("SpotLightComponent" + std::to_string(GetId()));
}

void UHSpotLightComponent::Update()
{
	if (!bIsEnabled)
	{
		return;
	}

	// light update doesn't need to calculate world matrix
	bTransformChanged = bIsWorldDirty;

	if (IsTransformChanged())
	{
		SetRenderDirties(true);
		bTransformChanged = false;
	}
}

void UHSpotLightComponent::SetRadius(float InRadius)
{
	Radius = InRadius;
}

float UHSpotLightComponent::GetRadius() const
{
	return Radius;
}

void UHSpotLightComponent::SetAngle(float InAngleDegree)
{
	Angle = std::abs(InAngleDegree);
	InnerAngle = Angle * 0.9f;
}

float UHSpotLightComponent::GetAngle() const
{
	return Angle;
}

UHSpotLightConstants UHSpotLightComponent::GetConstants() const
{
	UHSpotLightConstants Consts{};
	if (!bIsEnabled)
	{
		Consts.IsEnabled = 0;
		return Consts;
	}

	Consts.Color.x = LightColor.x;
	Consts.Color.y = LightColor.y;
	Consts.Color.z = LightColor.z;
	Consts.Radius = Radius;

	// half the angle when sending to GPU
	Consts.Angle = XMConvertToRadians(Angle * 0.5f);
	Consts.InnerAngle = XMConvertToRadians(InnerAngle * 0.5f);

	Consts.Color.x *= Intensity;
	Consts.Color.y *= Intensity;
	Consts.Color.z *= Intensity;
	Consts.Color.w = Intensity;

	Consts.Dir = Forward;
	Consts.Position = Position;

	return Consts;
}

#if WITH_EDITOR
UHDebugBoundConstant UHSpotLightComponent::GetDebugBoundConst() const
{
	UHDebugBoundConstant BoundConst{};
	BoundConst.BoundType = DebugCone;
	BoundConst.Position = GetPosition();
	BoundConst.Radius = GetRadius();
	BoundConst.Color = XMFLOAT3(1, 1, 0);
	BoundConst.Dir = Forward;
	BoundConst.Right = Right;
	BoundConst.Up = Up;
	BoundConst.Angle = XMConvertToRadians(Angle * 0.5f);

	return BoundConst;
}

void UHSpotLightComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();
	ImGui::NewLine();
	ImGui::Text("------ Spot Light ------");

	ImGui::InputFloat3("LightColor", (float*)&LightColor);
	ImGui::InputFloat("Intensity", &Intensity);
	ImGui::InputFloat("Radius", &Radius);
	ImGui::InputFloat("Angle", &Angle);
	InnerAngle = Angle * 0.9f;
}

void UHSpotLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
}

void UHSpotLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
}
#endif