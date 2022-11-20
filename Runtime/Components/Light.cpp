#include "Light.h"

UHDirectionalLightComponent::UHDirectionalLightComponent()
	: LightViewProj(MathHelpers::Identity4x4())
	, LightViewProjInv(MathHelpers::Identity4x4())
	, LightColor(XMFLOAT4())
	, ShadowReferencePoint(XMFLOAT3())
	, Intensity(1.0f)
	, ShadowRange(500)
	, ShadowDistance(1000)
{

}

void UHDirectionalLightComponent::Update()
{
	SetPosition(ShadowReferencePoint - Forward * ShadowDistance * 0.5f);
	UHTransformComponent::Update();

	if (IsTransformChanged())
	{
		BuildLightMatrix();
		SetRenderDirties(true);
	}
}

void UHDirectionalLightComponent::SetLightColor(XMFLOAT4 InLightColor)
{
	LightColor = InLightColor;
	SetRenderDirties(true);
}

void UHDirectionalLightComponent::SetIntensity(float InIntensity)
{
	Intensity = InIntensity;
	SetRenderDirties(true);
}

// set shadow range/distance used in orthographic matrix
void UHDirectionalLightComponent::SetShadowRange(float Range, float Distance)
{
	ShadowRange = Range;
	ShadowDistance = Distance;
}

// directional light will follow the reference point (usually camera) when building shadow matrix
void UHDirectionalLightComponent::SetShadowReferencePoint(XMFLOAT3 InPos)
{
	ShadowReferencePoint = InPos;
}

UHDirectionalLightConstants UHDirectionalLightComponent::GetConstants() const
{
	UHDirectionalLightConstants Consts;
	Consts.Color = LightColor;
	Consts.Dir = Forward;

	Consts.Color.x *= Intensity;
	Consts.Color.y *= Intensity;
	Consts.Color.z *= Intensity;

	return Consts;
}

XMFLOAT4X4 UHDirectionalLightComponent::GetLightViewProj() const
{
	return LightViewProj;
}

XMFLOAT4X4 UHDirectionalLightComponent::GetLightViewProjInv() const
{
	return LightViewProjInv;
}

void UHDirectionalLightComponent::BuildLightMatrix()
{
	// build light view matrix
	XMVECTOR U = -XMLoadFloat3(&Up);
	XMVECTOR F = XMLoadFloat3(&Forward);
	XMVECTOR P = XMLoadFloat3(&Position);

	XMMATRIX View = XMMatrixLookToRH(P, F, U);

	// build light projection matrix, reversed orthographic one
	XMMATRIX Proj = XMMatrixOrthographicRH(ShadowRange, ShadowRange, ShadowDistance, 0);

	XMMATRIX ViewProj = XMMatrixTranspose(Proj) * XMMatrixTranspose(View);
	XMStoreFloat4x4(&LightViewProj, ViewProj);

	// build inv view proj as well
	XMVECTOR Det = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Det, ViewProj);
	XMStoreFloat4x4(&LightViewProjInv, InvViewProj);
}