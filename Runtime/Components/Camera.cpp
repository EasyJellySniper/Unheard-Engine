#include "Camera.h"
#include "../CoreGlobals.h"

UHCameraComponent::UHCameraComponent()
	: NearPlane(0.1f)
	, Aspect(1.7778f)
	, FovY(XMConvertToRadians(60.0f))
	, ViewMatrix(MathHelpers::Identity4x4())
	, ProjectionMatrix(MathHelpers::Identity4x4())
	, ViewProjMatrix(MathHelpers::Identity4x4())
	, InvViewProjMatrix(MathHelpers::Identity4x4())
	, ProjectionMatrix_NonJittered(MathHelpers::Identity4x4())
	, ViewProjMatrix_NonJittered(MathHelpers::Identity4x4())
	, PrevViewProjMatrix_NonJittered(MathHelpers::Identity4x4())
	, InvViewProjMatrix_NonJittered(MathHelpers::Identity4x4())
	, bUseJitterOffset(false)
	, JitterOffset(XMFLOAT2())
	, Width(1)
	, Height(1)
	, CameraFrustum(BoundingFrustum())
	, CullingDistance(1000.0f)
{

}

void UHCameraComponent::Update()
{
	// must have latest transform info
	UHTransformComponent::Update();

	// should be fine to update camera every frame, camera nearly stops in a game
	BuildViewMatrix();
	BuildProjectionMatrix();

	// update ViewProj and inverse of it
	const XMMATRIX View = XMLoadFloat4x4(&ViewMatrix);
	const XMMATRIX Proj = XMLoadFloat4x4(&ProjectionMatrix);
	const XMMATRIX Proj_NonJittered = XMLoadFloat4x4(&ProjectionMatrix_NonJittered);
	const XMMATRIX ViewProj = XMMatrixTranspose(Proj) * XMMatrixTranspose(View);
	const XMMATRIX ViewProj_NonJittered = XMMatrixTranspose(Proj_NonJittered) * XMMatrixTranspose(View);

	// return transposed view proj matrix, also stores previous view proj before update
	PrevViewProjMatrix_NonJittered = ViewProjMatrix_NonJittered;
	XMStoreFloat4x4(&ViewProjMatrix, ViewProj);
	XMStoreFloat4x4(&ViewProjMatrix_NonJittered, ViewProj_NonJittered);

	XMVECTOR Det = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Det, ViewProj);
	XMStoreFloat4x4(&InvViewProjMatrix, InvViewProj);

	// non-jittered inv VP
	Det = XMMatrixDeterminant(ViewProj_NonJittered);
	InvViewProj = XMMatrixInverse(&Det, ViewProj_NonJittered);
	XMStoreFloat4x4(&InvViewProjMatrix_NonJittered, InvViewProj);

	// update camera frustum, note that this is a bit different than rendering matrix
	// in UHE, +X/+Y/+Z is used, so it needs to be left hand for culling
	const XMMATRIX CullingProj = XMMatrixPerspectiveFovLH(FovY, Aspect, NearPlane, CullingDistance);
	BoundingFrustum::CreateFromMatrix(CameraFrustum, CullingProj);

	// let frustum be in world space
	const XMFLOAT4X4& World = GetWorldMatrix();
	const XMMATRIX W = XMLoadFloat4x4(&World);
	CameraFrustum.Transform(CameraFrustum, XMMatrixTranspose(W));
}

void UHCameraComponent::SetNearPlane(float InNearZ)
{
	NearPlane = InNearZ;
}

void UHCameraComponent::SetAspect(float InAspect)
{
	Aspect = InAspect;
}

void UHCameraComponent::SetFov(float InFov)
{
	// store fov as radian
	FovY = XMConvertToRadians(InFov);
}

void UHCameraComponent::SetUseJitter(bool bInFlag)
{
	bUseJitterOffset = bInFlag;
}

void UHCameraComponent::SetResolution(int32_t RenderWidth, int32_t RenderHeight)
{
	Width = RenderWidth;
	Height = RenderHeight;
}

void UHCameraComponent::SetCullingDistance(float InDistance)
{
	CullingDistance = InDistance;
}

XMFLOAT4X4 UHCameraComponent::GetViewMatrix() const
{
	return ViewMatrix;
}

XMFLOAT4X4 UHCameraComponent::GetProjectionMatrix() const
{
	return ProjectionMatrix;
}

XMFLOAT4X4 UHCameraComponent::GetProjectionMatrixNonJittered() const
{
	return ProjectionMatrix_NonJittered;
}

XMFLOAT4X4 UHCameraComponent::GetViewProjMatrix() const
{	
	return ViewProjMatrix;
}

XMFLOAT4X4 UHCameraComponent::GetViewProjMatrixNonJittered() const
{
	return ViewProjMatrix_NonJittered;
}

XMFLOAT4X4 UHCameraComponent::GetPrevViewProjMatrixNonJittered() const
{
	return PrevViewProjMatrix_NonJittered;
}

XMFLOAT4X4 UHCameraComponent::GetInvViewProjMatrix() const
{
	return InvViewProjMatrix;
}

XMFLOAT4X4 UHCameraComponent::GetInvViewProjMatrixNonJittered() const
{
	return InvViewProjMatrix_NonJittered;
}

XMFLOAT2 UHCameraComponent::GetJitterOffset() const
{
	return JitterOffset;
}

BoundingFrustum UHCameraComponent::GetBoundingFrustum() const
{
	return CameraFrustum;
}

XMFLOAT3 UHCameraComponent::GetScreenPos(XMFLOAT3 InWorld) const
{
	XMVECTOR P = XMLoadFloat3(&InWorld);
	XMMATRIX VP = XMLoadFloat4x4(&ViewProjMatrix_NonJittered);

	// convert to clip pos
	P = XMVector3Transform(P, VP);

	// perspective divide
	P /= P.m128_f32[3];

	// [-1,1] to [0,1] and to screen pos
	P.m128_f32[0] = (P.m128_f32[0] * 0.5f + 0.5f) * static_cast<float>(Width);
	P.m128_f32[1] = (P.m128_f32[1] * 0.5f + 0.5f) * static_cast<float>(Height);

	XMFLOAT3 Result;
	XMStoreFloat3(&Result, P);
	return Result;
}

BoundingBox UHCameraComponent::GetScreenBound(BoundingBox InWorldBound) const
{
	// return screen space bound
	XMFLOAT3 P[8];
	InWorldBound.GetCorners(P);

	constexpr float Inf = std::numeric_limits<float>::infinity();
	XMFLOAT3 MinPoint = XMFLOAT3(Inf, Inf, Inf);
	XMFLOAT3 MaxPoint = XMFLOAT3(-Inf, -Inf, -Inf);

	for (int32_t Idx = 0; Idx < 8; Idx++)
	{
		P[Idx] = GetScreenPos(P[Idx]);
		MinPoint = MathHelpers::MinVector(P[Idx], MinPoint);
		MaxPoint = MathHelpers::MaxVector(P[Idx], MaxPoint);
	}

	BoundingBox Result;
	Result.Center = (MinPoint + MaxPoint) * 0.5f;
	Result.Extents = (MaxPoint - MinPoint) * 0.5f;
	return Result;
}

void UHCameraComponent::BuildViewMatrix()
{
	// flip up vector since Vulkan want +Y down, but UH uses +Y up
	const XMVECTOR U = -XMLoadFloat3(&Up);
	const XMVECTOR F = XMLoadFloat3(&Forward);
	const XMVECTOR P = XMLoadFloat3(&Position);

	const XMMATRIX View = XMMatrixLookToRH(P, F, U);
	XMStoreFloat4x4(&ViewMatrix, View);
}

void UHCameraComponent::BuildProjectionMatrix()
{
	// based on right handed system
	// far z value doesn't matter here, since I'll use infinte far plane
	const XMMATRIX P = XMMatrixPerspectiveFovRH(FovY, Aspect, NearPlane + 1, NearPlane);
	XMStoreFloat4x4(&ProjectionMatrix_NonJittered, P);
	XMStoreFloat4x4(&ProjectionMatrix, P);

	// apply jitter offset if it's enabled
	if (bUseJitterOffset)
	{
		// offset half pixel
		const XMFLOAT2 Offset = XMFLOAT2(MathHelpers::Halton(GFrameNumber & 511, 2), MathHelpers::Halton(GFrameNumber & 511, 3));
		JitterOffset.x = Offset.x / Width;
		JitterOffset.y = Offset.y / Height;

		const XMMATRIX JitterMatrix = XMMatrixTranslation(JitterOffset.x, JitterOffset.y, 0);
		XMStoreFloat4x4(&ProjectionMatrix, P * JitterMatrix);
	}

	// adjust matrix value for infinite far plane
	ProjectionMatrix_NonJittered(2, 2) = 0.0f;
	ProjectionMatrix_NonJittered(3, 2) = NearPlane;
	ProjectionMatrix(2, 2) = 0.0f;
	ProjectionMatrix(3, 2) = NearPlane;
}