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

	void SetSkyColor(UHVector3 InColor);
	void SetGroundColor(UHVector3 InColor);
	void SetSkyIntensity(float InIntensity);
	void SetGroundIntensity(float InIntensity);

	UHVector3 GetSkyColor() const;
	UHVector3 GetGroundColor() const;
	float GetSkyIntensity() const;
	float GetGroundIntensity() const;

	void SetCubemap(UHTextureCube* InCube);
	UHTextureCube* GetCubemap() const;

#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
#endif

private:
	// simply provides ambient sky/ground color for now, will consider SH method in the future
	UHVector3 AmbientSkyColor;
	UHVector3 AmbientGroundColor;
	float SkyIntensity;
	float GroundIntensity;
	UHTextureCube* CubemapCache;
	UUID CubemapId;
};