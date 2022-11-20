#pragma once
#include "../Renderer/RenderingTypes.h"
#include "Transform.h"

// directional lighting component, this cares direction only
class UHDirectionalLightComponent : public UHTransformComponent, public UHRenderState
{
public:
	UHDirectionalLightComponent();
	virtual void Update() override;

	void SetLightColor(XMFLOAT4 InLightColor);
	void SetIntensity(float InIntensity);
	void SetShadowRange(float Range, float Distance);
	void SetShadowReferencePoint(XMFLOAT3 InPos);

	UHDirectionalLightConstants GetConstants() const;
	XMFLOAT4X4 GetLightViewProj() const;
	XMFLOAT4X4 GetLightViewProjInv() const;

private:
	void BuildLightMatrix();

	XMFLOAT4X4 LightViewProj;
	XMFLOAT4X4 LightViewProjInv;
	XMFLOAT4 LightColor;
	XMFLOAT3 ShadowReferencePoint;
	float Intensity;
	float ShadowRange;
	float ShadowDistance;
};