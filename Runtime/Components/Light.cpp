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
void UHDirectionalLightComponent::OnPropertyChange(std::string PropertyName)
{
	UHTransformComponent::OnPropertyChange(PropertyName);

	const UHFloat3DetailGUI* Float3GUI = static_cast<const UHFloat3DetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHFloatDetailGUI* FloatGUI = static_cast<const UHFloatDetailGUI*>(DetailView->GetGUI(PropertyName));

	if (PropertyName == "LightColor")
	{
		LightColor = Float3GUI->GetValue();
	}
	else if (PropertyName == "Intensity")
	{
		Intensity = FloatGUI->GetValue();
	}
}

void UHDirectionalLightComponent::OnGenerateDetailView(HWND ParentWnd)
{
	UHTransformComponent::OnGenerateDetailView(ParentWnd);

	DetailView = MakeUnique<UHDetailView>("Directional Light");
	DetailView->OnGenerateDetailView<UHDirectionalLightComponent>(this, ParentWnd, DetailStartHeight);
	UH_SYNC_DETAIL_VALUE("LightColor", LightColor)
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
}

void UHDirectionalLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
	UH_SYNC_DETAIL_VALUE("LightColor", InLightColor)
}

void UHDirectionalLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
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
	UH_SYNC_DETAIL_VALUE("Radius", Radius)
}

float UHPointLightComponent::GetRadius() const
{
	return Radius;
}

UHPointLightConstants UHPointLightComponent::GetConstants() const
{
	UHPointLightConstants Consts{};
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

void UHPointLightComponent::OnPropertyChange(std::string PropertyName)
{
	UHTransformComponent::OnPropertyChange(PropertyName);

	const UHFloat3DetailGUI* Float3GUI = static_cast<const UHFloat3DetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHFloatDetailGUI* FloatGUI = static_cast<const UHFloatDetailGUI*>(DetailView->GetGUI(PropertyName));

	if (PropertyName == "LightColor")
	{
		LightColor = Float3GUI->GetValue();
	}
	else if (PropertyName == "Intensity")
	{
		Intensity = FloatGUI->GetValue();
	}
	else if (PropertyName == "Radius")
	{
		Radius = FloatGUI->GetValue();
	}
}

void UHPointLightComponent::OnGenerateDetailView(HWND ParentWnd)
{
	UHTransformComponent::OnGenerateDetailView(ParentWnd);

	DetailView = MakeUnique<UHDetailView>("Point Light");
	DetailView->OnGenerateDetailView<UHPointLightComponent>(this, ParentWnd, DetailStartHeight);
	UH_SYNC_DETAIL_VALUE("LightColor", LightColor)
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
	UH_SYNC_DETAIL_VALUE("Radius", Radius)
}

void UHPointLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
	UH_SYNC_DETAIL_VALUE("LightColor", LightColor)
}

void UHPointLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
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
	UH_SYNC_DETAIL_VALUE("Radius", InRadius)
}

float UHSpotLightComponent::GetRadius() const
{
	return Radius;
}

void UHSpotLightComponent::SetAngle(float InAngleDegree)
{
	Angle = std::abs(InAngleDegree);
	InnerAngle = Angle * 0.9f;
	UH_SYNC_DETAIL_VALUE("Angle", InAngleDegree)
}

float UHSpotLightComponent::GetAngle() const
{
	return Angle;
}

UHSpotLightConstants UHSpotLightComponent::GetConstants() const
{
	UHSpotLightConstants Consts{};

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

void UHSpotLightComponent::OnPropertyChange(std::string PropertyName)
{
	UHTransformComponent::OnPropertyChange(PropertyName);

	const UHFloat3DetailGUI* Float3GUI = static_cast<const UHFloat3DetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHFloatDetailGUI* FloatGUI = static_cast<const UHFloatDetailGUI*>(DetailView->GetGUI(PropertyName));

	if (PropertyName == "LightColor")
	{
		LightColor = Float3GUI->GetValue();
	}
	else if (PropertyName == "Intensity")
	{
		Intensity = FloatGUI->GetValue();
	}
	else if (PropertyName == "Radius")
	{
		Radius = FloatGUI->GetValue();
	}
	else if (PropertyName == "Angle")
	{
		Angle = FloatGUI->GetValue();
		InnerAngle = Angle * 0.9f;
	}
}

void UHSpotLightComponent::OnGenerateDetailView(HWND ParentWnd)
{
	UHTransformComponent::OnGenerateDetailView(ParentWnd);

	DetailView = MakeUnique<UHDetailView>("Spot Light");
	DetailView->OnGenerateDetailView<UHSpotLightComponent>(this, ParentWnd, DetailStartHeight);
	UH_SYNC_DETAIL_VALUE("LightColor", LightColor)
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
	UH_SYNC_DETAIL_VALUE("Radius", Radius)
	UH_SYNC_DETAIL_VALUE("Angle", Angle)
}

void UHSpotLightComponent::SetLightColor(XMFLOAT3 InLightColor)
{
	UHLightBase::SetLightColor(InLightColor);
	UH_SYNC_DETAIL_VALUE("LightColor", LightColor)
}

void UHSpotLightComponent::SetIntensity(float InIntensity)
{
	UHLightBase::SetIntensity(InIntensity);
	UH_SYNC_DETAIL_VALUE("Intensity", Intensity)
}
#endif