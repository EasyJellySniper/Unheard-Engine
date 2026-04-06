#include "Camera.h"
#include "../CoreGlobals.h"

UHCameraComponent::UHCameraComponent()
	: NearPlane(0.1f)
	, Aspect(1.7778f)
	, FovY(UHMathHelpers::ToRadians(60.0f))
	, ViewMatrix(UHMathHelpers::Identity4x4())
	, ProjectionMatrix(UHMathHelpers::Identity4x4())
	, ViewProjMatrix(UHMathHelpers::Identity4x4())
	, InvViewProjMatrix(UHMathHelpers::Identity4x4())
	, ProjectionMatrix_NonJittered(UHMathHelpers::Identity4x4())
	, ViewProjMatrix_NonJittered(UHMathHelpers::Identity4x4())
	, PrevViewProjMatrix_NonJittered(UHMathHelpers::Identity4x4())
	, InvViewProjMatrix_NonJittered(UHMathHelpers::Identity4x4())
	, InvProjMatrix(UHMathHelpers::Identity4x4())
	, InvProjMatrix_NonJittered(UHMathHelpers::Identity4x4())
	, bUseJitterOffset(false)
	, JitterOffset(UHVector2())
	, Width(1)
	, Height(1)
	, CameraFrustum(UHBoundingFrustum())
	, CullingDistance(1000.0f)
	, JitterScaleMin(0.01f)
	, JitterScaleMax(1.0f)
	, JitterEndDistance(500.0f)
#if WITH_EDITOR
	, FovYDeg(60.0f)
#endif
{
	SetName("CameraComponent" + std::to_string(GetId()));
	ObjectClassIdInternal = ClassId;
}

void UHCameraComponent::Update()
{
	if (!bIsEnabled)
	{
		return;
	}

	// must have latest transform info
	UHTransformComponent::Update();

	// should be fine to update camera every frame, camera nearly stops in a game
	BuildViewMatrix();
	BuildProjectionMatrix();

	// update ViewProj and transpose of it
	const UHMatrix4x4 View = ViewMatrix;
	const UHMatrix4x4 Proj = ProjectionMatrix;
	const UHMatrix4x4 Proj_NonJittered = ProjectionMatrix_NonJittered;
	const UHMatrix4x4 ViewProj = UHMathHelpers::UHMatrixTranspose(Proj) * UHMathHelpers::UHMatrixTranspose(View);
	const UHMatrix4x4 ViewProj_NonJittered = UHMathHelpers::UHMatrixTranspose(Proj_NonJittered) * UHMathHelpers::UHMatrixTranspose(View);

	// return transposed view proj matrix, also stores previous view proj before update
	PrevViewProjMatrix_NonJittered = ViewProjMatrix_NonJittered;
	ViewProjMatrix = ViewProj;
	ViewProjMatrix_NonJittered = ViewProj_NonJittered;

	// inv view-proj calculation
	UHVector4 Det = UHMathHelpers::UHMatrixDeterminant(ViewProj);
	UHMatrix4x4 InvViewProj = UHMathHelpers::UHMatrixInverse(Det, ViewProj);
	InvViewProjMatrix = InvViewProj;

	// non-jittered inv VP
	Det = UHMathHelpers::UHMatrixDeterminant(ViewProj_NonJittered);
	InvViewProj = UHMathHelpers::UHMatrixInverse(Det, ViewProj_NonJittered);
	InvViewProjMatrix_NonJittered = InvViewProj;

	// inv proj
	Det = UHMathHelpers::UHMatrixDeterminant(Proj);
	UHMatrix4x4 InvProj = UHMathHelpers::UHMatrixInverse(Det, Proj);
	InvProjMatrix = InvProj;

	Det = UHMathHelpers::UHMatrixDeterminant(Proj_NonJittered);
	UHMatrix4x4 InvProj_NonJittered = UHMathHelpers::UHMatrixInverse(Det, Proj_NonJittered);
	InvProjMatrix_NonJittered = InvProj_NonJittered;

	// update camera frustum, note that this is a bit different than rendering matrix
	// in UHE, +X/+Y/+Z is used, so it needs to be left hand for culling
	const UHMatrix4x4 CullingProj = UHMathHelpers::UHMatrixPerspectiveFovLH(FovY, Aspect, NearPlane, CullingDistance);
	UHBoundingFrustum::CreateFromMatrix(CameraFrustum, CullingProj);

	// let frustum be in world space
	const UHMatrix4x4& World = GetWorldMatrix();
	CameraFrustum.Transform(CameraFrustum, UHMathHelpers::UHMatrixTranspose(World));
}

void UHCameraComponent::OnSave(std::ofstream& OutStream)
{
	UHComponent::OnSave(OutStream);
	UHTransformComponent::OnSave(OutStream);
	OutStream.write(reinterpret_cast<const char*>(&NearPlane), sizeof(NearPlane));
#if WITH_EDITOR
	OutStream.write(reinterpret_cast<const char*>(&FovYDeg), sizeof(FovYDeg));
#else
	float Dummy = 60.0f;
	OutStream.write(reinterpret_cast<const char*>(&Dummy), sizeof(Dummy));
#endif
	OutStream.write(reinterpret_cast<const char*>(&CullingDistance), sizeof(CullingDistance));
	OutStream.write(reinterpret_cast<const char*>(&JitterScaleMin), sizeof(JitterScaleMin));
	OutStream.write(reinterpret_cast<const char*>(&JitterScaleMax), sizeof(JitterScaleMax));
	OutStream.write(reinterpret_cast<const char*>(&JitterEndDistance), sizeof(JitterEndDistance));
}

void UHCameraComponent::OnLoad(std::ifstream& InStream)
{
	UHComponent::OnLoad(InStream);
	UHTransformComponent::OnLoad(InStream);
	InStream.read(reinterpret_cast<char*>(&NearPlane), sizeof(NearPlane));
#if WITH_EDITOR
	InStream.read(reinterpret_cast<char*>(&FovYDeg), sizeof(FovYDeg));
#else
	float Dummy = 0.0f;
	InStream.read(reinterpret_cast<char*>(&Dummy), sizeof(Dummy));
#endif
	InStream.read(reinterpret_cast<char*>(&CullingDistance), sizeof(CullingDistance));
	InStream.read(reinterpret_cast<char*>(&JitterScaleMin), sizeof(JitterScaleMin));
	InStream.read(reinterpret_cast<char*>(&JitterScaleMax), sizeof(JitterScaleMax));
	InStream.read(reinterpret_cast<char*>(&JitterEndDistance), sizeof(JitterEndDistance));
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
	FovY = UHMathHelpers::ToRadians(InFov);
#if WITH_EDITOR
	FovYDeg = InFov;
#endif
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

UHMatrix4x4 UHCameraComponent::GetViewMatrix() const
{
	return ViewMatrix;
}

UHMatrix4x4 UHCameraComponent::GetProjectionMatrix() const
{
	return ProjectionMatrix;
}

UHMatrix4x4 UHCameraComponent::GetProjectionMatrixNonJittered() const
{
	return ProjectionMatrix_NonJittered;
}

UHMatrix4x4 UHCameraComponent::GetViewProjMatrix() const
{	
	return ViewProjMatrix;
}

UHMatrix4x4 UHCameraComponent::GetViewProjMatrixNonJittered() const
{
	return ViewProjMatrix_NonJittered;
}

UHMatrix4x4 UHCameraComponent::GetPrevViewProjMatrixNonJittered() const
{
	return PrevViewProjMatrix_NonJittered;
}

UHMatrix4x4 UHCameraComponent::GetInvViewProjMatrix() const
{
	return InvViewProjMatrix;
}

UHMatrix4x4 UHCameraComponent::GetInvViewProjMatrixNonJittered() const
{
	return InvViewProjMatrix_NonJittered;
}

UHMatrix4x4 UHCameraComponent::GetInvProjMatrix() const
{
	return InvProjMatrix;
}

UHMatrix4x4 UHCameraComponent::GetInvProjMatrixNonJittered() const
{
	return InvProjMatrix_NonJittered;
}

UHVector4 UHCameraComponent::GetJitterOffset() const
{
	UHVector4 Offset;
	Offset.X = JitterOffset.X;
	Offset.Y = JitterOffset.Y;
	Offset.Z = JitterScaleMin;
	Offset.W = 1.0f / JitterEndDistance;

	return Offset;
}

UHBoundingFrustum UHCameraComponent::GetBoundingFrustum() const
{
	return CameraFrustum;
}

UHVector3 UHCameraComponent::GetScreenPos(UHVector3 InWorld) const
{
	// convert to clip pos
	UHVector4 ClipPos = UHMathHelpers::UHVector3Transform(InWorld, ViewProjMatrix_NonJittered);

	// perspective divide
	ClipPos /= ClipPos.W;

	// [-1,1] to [0,1] and to screen pos
	ClipPos.X = (ClipPos.X * 0.5f + 0.5f) * static_cast<float>(Width);
	ClipPos.Y = (ClipPos.Y * 0.5f + 0.5f) * static_cast<float>(Height);

	return UHVector3(ClipPos.X, ClipPos.Y, ClipPos.Z);
}

UHBoundingBox UHCameraComponent::GetScreenBound(UHBoundingBox InWorldBound) const
{
	// return screen space bound
	UHVector3 P[8];
	InWorldBound.GetCorners(P);

	constexpr float Inf = std::numeric_limits<float>::infinity();
	UHVector3 MinPoint = UHVector3(Inf, Inf, Inf);
	UHVector3 MaxPoint = UHVector3(-Inf, -Inf, -Inf);

	for (int32_t Idx = 0; Idx < 8; Idx++)
	{
		P[Idx] = GetScreenPos(P[Idx]);
		MinPoint = UHMathHelpers::MinVector(P[Idx], MinPoint);
		MaxPoint = UHMathHelpers::MaxVector(P[Idx], MaxPoint);
	}

	UHBoundingBox Result;
	Result.Center = (MinPoint + MaxPoint) * 0.5f;
	Result.Extents = (MaxPoint - MinPoint) * 0.5f;
	return Result;
}

float UHCameraComponent::GetCullingDistance() const
{
	return CullingDistance;
}

float UHCameraComponent::GetNearPlane() const
{
	return NearPlane;
}

#if WITH_EDITOR
void UHCameraComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();

	ImGui::NewLine();
	ImGui::Text("------ Camera ------");

	if (ImGui::InputFloat("NearPlane", &NearPlane))
	{
		// can't be zero near plane
		NearPlane = max(NearPlane, 0.01f);
		SetNearPlane(NearPlane);
	}

	if (ImGui::InputFloat("FovY", &FovYDeg))
	{
		// can't be zero FOV
		FovYDeg = max(FovYDeg, 1.0f);
		SetFov(FovYDeg);
	}

	if (ImGui::InputFloat("CullingDistance", &CullingDistance))
	{
		SetCullingDistance(CullingDistance);
	}

	ImGui::InputFloat("GJitterScaleMin", &JitterScaleMin);
	ImGui::InputFloat("JitterScaleMax", &JitterScaleMax);
	ImGui::InputFloat("JitterEndDistance", &JitterEndDistance);
}

#endif

void UHCameraComponent::BuildViewMatrix()
{
	// flip up vector since Vulkan want +Y down, but UH uses +Y up
	ViewMatrix = UHMathHelpers::UHMatrixLookToRH(Position, Forward, Up * -1);
}

void UHCameraComponent::BuildProjectionMatrix()
{
	// based on right handed system
	// far z value doesn't matter here, since I'll use infinte far plane
	const UHMatrix4x4 P = UHMathHelpers::UHMatrixPerspectiveFovRH(FovY, Aspect, NearPlane + 1, NearPlane);
	ProjectionMatrix_NonJittered = P;
	ProjectionMatrix = P;

	// apply jitter offset if it's enabled
	if (bUseJitterOffset)
	{
		const UHVector2 Offset = UHVector2(UHMathHelpers::Halton(GFrameNumber & 511, 2), UHMathHelpers::Halton(GFrameNumber & 511, 3));
		JitterOffset.X = Offset.X / Width;
		JitterOffset.Y = Offset.Y / Height;

		// when standing still, use lower jitter offset scale to prevent flickering
		JitterOffset.X *= bIsWorldDirty ? JitterScaleMax : JitterScaleMin;
		JitterOffset.Y *= bIsWorldDirty ? JitterScaleMax : JitterScaleMin;

		const UHMatrix4x4 JitterMatrix = UHMathHelpers::UHMatrixTranslation(UHVector3(JitterOffset.X, JitterOffset.Y, 0));
		ProjectionMatrix = P * JitterMatrix;
	}

	// adjust matrix value for infinite far plane
	ProjectionMatrix_NonJittered(2, 2) = 0.0f;
	ProjectionMatrix_NonJittered(3, 2) = NearPlane;
	ProjectionMatrix(2, 2) = 0.0f;
	ProjectionMatrix(3, 2) = NearPlane;
}