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
	UHDirectionalLightComponent();
	virtual void Update() override;

	UHDirectionalLightConstants GetConstants() const;
#if WITH_EDITOR
	virtual void OnPropertyChange(std::string PropertyName) override;
	virtual void OnGenerateDetailView(HWND ParentWnd) override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;

	UH_BEGIN_REFLECT
	UH_MEMBER_REFLECT("XMFLOAT3", "LightColor")
	UH_MEMBER_REFLECT("float", "Intensity")
	UH_END_REFLECT
#endif

private:
#if WITH_EDITOR
	UniquePtr<UHDetailView> DetailView;
#endif
};

// point light component, has the radius info, use square attenuation for now
class UHPointLightComponent : public UHLightBase
{
public:
	UHPointLightComponent();
	virtual void Update() override;

	void SetRadius(float InRadius);
	float GetRadius() const;

	UHPointLightConstants GetConstants() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnPropertyChange(std::string PropertyName) override;
	virtual void OnGenerateDetailView(HWND ParentWnd) override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;

	UH_BEGIN_REFLECT
	UH_MEMBER_REFLECT("XMFLOAT3", "LightColor")
	UH_MEMBER_REFLECT("float", "Intensity")
	UH_MEMBER_REFLECT("float", "Radius")
	UH_END_REFLECT
#endif

private:
	float Radius;
#if WITH_EDITOR
	UniquePtr<UHDetailView> DetailView;
#endif
};

// spot light component, has the radius and angle info, use square attenuation for now
class UHSpotLightComponent : public UHLightBase
{
public:
	UHSpotLightComponent();
	virtual void Update() override;

	void SetRadius(float InRadius);
	float GetRadius() const;

	void SetAngle(float InAngleDegree);
	float GetAngle() const;

	UHSpotLightConstants GetConstants() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnPropertyChange(std::string PropertyName) override;
	virtual void OnGenerateDetailView(HWND ParentWnd) override;
	virtual void SetLightColor(XMFLOAT3 InLightColor) override;
	virtual void SetIntensity(float InIntensity) override;

	UH_BEGIN_REFLECT
		UH_MEMBER_REFLECT("XMFLOAT3", "LightColor")
		UH_MEMBER_REFLECT("float", "Intensity")
		UH_MEMBER_REFLECT("float", "Radius")
		UH_MEMBER_REFLECT("float", "Angle")
	UH_END_REFLECT
#endif

private:
	float Radius;
	float Angle;
	float InnerAngle;

#if WITH_EDITOR
	UniquePtr<UHDetailView> DetailView;
#endif
};