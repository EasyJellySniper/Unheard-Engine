#pragma once
#include "../Classes/Types.h"
#include "Transform.h"
#include "../Classes/TextureCube.h"
#include "../Engine/Asset.h"

// UH Skylight component
class UHSkyLightComponent : public UHTransformComponent
{
public:
	STATIC_CLASS_ID(99222499)
	UHSkyLightComponent();
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;
	virtual void OnPostLoad(UHAssetManager* InAssetMgr) override;
	virtual void OnActivityChanged() override;

	void SetSkyColor(XMFLOAT3 InColor);
	void SetGroundColor(XMFLOAT3 InColor);
	void SetSkyIntensity(float InIntensity);
	void SetGroundIntensity(float InIntensity);

	XMFLOAT3 GetSkyColor() const;
	XMFLOAT3 GetGroundColor() const;
	float GetSkyIntensity() const;
	float GetGroundIntensity() const;

	void SetCubemap(UHTextureCube* InCube);
	UHTextureCube* GetCubemap() const;

#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
#endif

private:
	// simply provides ambient sky/ground color for now, will consider SH method in the future
	XMFLOAT3 AmbientSkyColor;
	XMFLOAT3 AmbientGroundColor;
	float SkyIntensity;
	float GroundIntensity;
	UHTextureCube* CubemapCache;
	UUID CubemapId;
};