#pragma once
#include "../Classes/Types.h"

// UH Skylight component
class UHSkyLightComponent
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

private:
	// simply provides ambient sky/ground color for now, will consider SH method in the future
	XMFLOAT3 AmbientSkyColor;
	XMFLOAT3 AmbientGroundColor;
	float SkyIntensity;
	float GroundIntensity;
};