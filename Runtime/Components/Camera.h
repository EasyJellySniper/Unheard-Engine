#pragma once
#include "Transform.h"

// class for camera, this one will hold view/projection matrix info for use
// UH uses reversed infinite far plane for projection matrix, so there is no far plane parameter here
class UHCameraComponent : public UHTransformComponent
{
public:
	STATIC_CLASS_ID(99446654)
	UHCameraComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

	void SetNearPlane(float InNearZ);
	void SetAspect(float InAspect);
	void SetFov(float InFov);
	void SetUseJitter(bool bInFlag);
	void SetResolution(int32_t RenderWidth, int32_t RenderHeight);
	void SetCullingDistance(float InDistance);

	UHMatrix4x4 GetViewMatrix() const;
	UHMatrix4x4 GetProjectionMatrix() const;
	UHMatrix4x4 GetProjectionMatrixNonJittered() const;
	UHMatrix4x4 GetViewProjMatrix() const;
	UHMatrix4x4 GetViewProjMatrixNonJittered() const;
	UHMatrix4x4 GetPrevViewProjMatrixNonJittered() const;
	UHMatrix4x4 GetInvViewProjMatrix() const;
	UHMatrix4x4 GetInvViewProjMatrixNonJittered() const;
	UHMatrix4x4 GetInvProjMatrix() const;
	UHMatrix4x4 GetInvProjMatrixNonJittered() const;
	UHVector4 GetJitterOffset() const;
	UHBoundingFrustum GetBoundingFrustum() const;
	UHVector3 GetScreenPos(UHVector3 InWorld) const;
	UHBoundingBox GetScreenBound(UHBoundingBox InWorldBound) const;
	float GetCullingDistance() const;
	float GetNearPlane() const;

#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
#endif

private:
	void BuildViewMatrix();
	void BuildProjectionMatrix();

	float NearPlane;
	float Aspect;
	float FovY;
#if WITH_EDITOR
	float FovYDeg;
#endif

	UHMatrix4x4 ViewMatrix;
	UHMatrix4x4 ProjectionMatrix;
	UHMatrix4x4 ProjectionMatrix_NonJittered;
	UHMatrix4x4 ViewProjMatrix;
	UHMatrix4x4 ViewProjMatrix_NonJittered;
	UHMatrix4x4 PrevViewProjMatrix_NonJittered;
	UHMatrix4x4 InvViewProjMatrix;
	UHMatrix4x4 InvViewProjMatrix_NonJittered;
	UHMatrix4x4 InvProjMatrix;
	UHMatrix4x4 InvProjMatrix_NonJittered;

	// for temporal effects
	bool bUseJitterOffset;
	int32_t Width;
	int32_t Height;
	UHVector2 JitterOffset;
	float JitterScaleMin;
	float JitterScaleMax;
	float JitterEndDistance;

	UHBoundingFrustum CameraFrustum;
	float CullingDistance;
};