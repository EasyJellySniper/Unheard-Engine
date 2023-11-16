#pragma once
#include "../Renderer/RenderingTypes.h"
#include "Transform.h"

// shared light base class
class UHLightBase : public UHTransformComponent, public UHRenderState
{
public:
	UHLightBase();
	virtual ~UHLightBase() {}

	virtual void SetLightColor(XMFLOAT3 InLightColor);
	virtual void SetIntensity(float InIntensity);

protected:
	XMFLOAT3 LightColor;
	float Intensity;
};

// directional lighting component, this cares direction only, which can be obtained from transform component
class UHDirectionalLightComponent : public UHLightBase
{
public:
	STATIC_CLASS_ID(72086964)
	UHDirectionalLightComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

	UHDirectionalLightConstants GetConstants() const;
#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;
#endif
};

// point light component, has the radius info, use square attenuation for now
class UHPointLightComponent : public UHLightBase
{
public:
	STATIC_CLASS_ID(49002389)
	UHPointLightComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

	void SetRadius(float InRadius);
	float GetRadius() const;

	UHPointLightConstants GetConstants() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnGenerateDetailView() override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;
#endif

private:
	float Radius;
};

// spot light component, has the radius and angle info, use square attenuation for now
class UHSpotLightComponent : public UHLightBase
{
public:
	STATIC_CLASS_ID(69402010)
	UHSpotLightComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

	void SetRadius(float InRadius);
	float GetRadius() const;

	void SetAngle(float InAngleDegree);
	float GetAngle() const;

	UHSpotLightConstants GetConstants() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnGenerateDetailView() override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;
#endif

private:
	float Radius;
	float Angle;
	float InnerAngle;
};