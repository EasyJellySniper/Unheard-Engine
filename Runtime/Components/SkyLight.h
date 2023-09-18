#pragma once
#include "../Classes/Types.h"
#include "Transform.h"

// UH Skylight component
class UHSkyLightComponent : public UHTransformComponent
{
public:
	UHSkyLightComponent();

	void SetSkyColor(XMFLOAT3 InColor);
	void SetGroundColor(XMFLOAT3 InColor);
	void SetSkyIntensity(float InIntensity);
	void SetGroundIntensity(float InIntensity);

	XMFLOAT3 GetSkyColor() const;
	XMFLOAT3 GetGroundColor() const;
	float GetSkyIntensity() const;
	float GetGroundIntensity() const;

#if WITH_EDITOR
	virtual void OnPropertyChange(std::string PropertyName) override;
	virtual void OnGenerateDetailView(HWND ParentWnd) override;

	UH_BEGIN_REFLECT
	UH_MEMBER_REFLECT("XMFLOAT3", "AmbientSky")
	UH_MEMBER_REFLECT("XMFLOAT3", "AmbientGround")
	UH_MEMBER_REFLECT("float", "SkyIntensity")
	UH_MEMBER_REFLECT("float", "GroundIntensity")
	UH_END_REFLECT
#endif

private:
	// simply provides ambient sky/ground color for now, will consider SH method in the future
	XMFLOAT3 AmbientSkyColor;
	XMFLOAT3 AmbientGroundColor;
	float SkyIntensity;
	float GroundIntensity;

#if WITH_EDITOR
	UniquePtr<UHDetailView> DetailView;
#endif
};