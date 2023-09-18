#pragma once
#include "Transform.h"

// class for camera, this one will hold view/projection matrix info for use
// UH uses reversed infinite far plane for projection matrix, so there is no far plane parameter here
class UHCameraComponent : public UHTransformComponent
{
public:
	UHCameraComponent();
	virtual void Update() override;

	void SetNearPlane(float InNearZ);
	void SetAspect(float InAspect);
	void SetFov(float InFov);
	void SetUseJitter(bool bInFlag);
	void SetResolution(int32_t RenderWidth, int32_t RenderHeight);
	void SetCullingDistance(float InDistance);

	XMFLOAT4X4 GetViewMatrix() const;
	XMFLOAT4X4 GetProjectionMatrix() const;
	XMFLOAT4X4 GetProjectionMatrixNonJittered() const;
	XMFLOAT4X4 GetViewProjMatrix() const;
	XMFLOAT4X4 GetViewProjMatrixNonJittered() const;
	XMFLOAT4X4 GetPrevViewProjMatrixNonJittered() const;
	XMFLOAT4X4 GetInvViewProjMatrix() const;
	XMFLOAT4X4 GetInvViewProjMatrixNonJittered() const;
	XMFLOAT4X4 GetInvProjMatrix() const;
	XMFLOAT4X4 GetInvProjMatrixNonJittered() const;
	XMFLOAT4 GetJitterOffset() const;
	BoundingFrustum GetBoundingFrustum() const;
	XMFLOAT3 GetScreenPos(XMFLOAT3 InWorld) const;
	BoundingBox GetScreenBound(BoundingBox InWorldBound) const;

#if WITH_EDITOR
	virtual void OnPropertyChange(std::string PropertyName) override;
	virtual void OnGenerateDetailView(HWND ParentWnd) override;

	UH_BEGIN_REFLECT
	UH_MEMBER_REFLECT("float", "NearPlane")
	UH_MEMBER_REFLECT("float", "FovY")
	UH_MEMBER_REFLECT("float", "CullingDistance")
	UH_MEMBER_REFLECT("float", "JitterScaleMin")
	UH_MEMBER_REFLECT("float", "JitterScaleMax")
	UH_MEMBER_REFLECT("float", "JitterEndDistance")
	UH_END_REFLECT
#endif

private:
	void BuildViewMatrix();
	void BuildProjectionMatrix();

	float NearPlane;
	float Aspect;
	float FovY;

	XMFLOAT4X4 ViewMatrix;
	XMFLOAT4X4 ProjectionMatrix;
	XMFLOAT4X4 ProjectionMatrix_NonJittered;
	XMFLOAT4X4 ViewProjMatrix;
	XMFLOAT4X4 ViewProjMatrix_NonJittered;
	XMFLOAT4X4 PrevViewProjMatrix_NonJittered;
	XMFLOAT4X4 InvViewProjMatrix;
	XMFLOAT4X4 InvViewProjMatrix_NonJittered;
	XMFLOAT4X4 InvProjMatrix;
	XMFLOAT4X4 InvProjMatrix_NonJittered;

	// for temporal effects
	bool bUseJitterOffset;
	int32_t Width;
	int32_t Height;
	XMFLOAT2 JitterOffset;
	float JitterScaleMin;
	float JitterScaleMax;
	float JitterEndDistance;

	BoundingFrustum CameraFrustum;
	float CullingDistance;

#if WITH_EDITOR
	UniquePtr<UHDetailView> DetailView;
#endif
};