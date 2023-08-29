#pragma once
#include "../Renderer/RenderingTypes.h"
#include "Transform.h"

// shared light base class
class UHLightBase : public UHTransformComponent, public UHRenderState
{
public:
	UHLightBase();
	virtual ~UHLightBase() {}

	void SetLightColor(XMFLOAT3 InLightColor);
	void SetIntensity(float InIntensity);

protected:
	XMFLOAT3 LightColor;
	float Intensity;
};

// directional lighting component, this cares direction only, which can be obtained from transform component
class UHDirectionalLightComponent : public UHLightBase
{
public:
	UHDirectionalLightComponent() {}
	virtual void Update() override;

	UHDirectionalLightConstants GetConstants() const;
};

// point light component, has the radius info, use square attenuation for now
class UHPointLightComponent : public UHLightBase
{
public:
	UHPointLightComponent();
	virtual void Update() override;

	void SetRadius(float InRadius);

	UHPointLightConstants GetConstants() const;

private:
	float Radius;
};