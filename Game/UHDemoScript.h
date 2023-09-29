#pragma once
#include "../Runtime/Components/GameScript.h"
#include "../Runtime/Components/Light.h"
#include "../Runtime/Components/SkyLight.h"
#include "../Runtime/Components/MeshRenderer.h"

enum UHDemoType
{
	DayTest,
	PointLightNight,
	SpotLightNight
};

// UH demo script
class UHDemoScript : public UHGameScript
{
public:
	UHDemoScript();

	virtual void OnEngineUpdate(float DeltaTime) override;
	virtual void OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx) override;

private:
	UHDirectionalLightComponent DefaultDirectionalLight;
	UHDirectionalLightComponent SecondDirectionalLight;
	std::vector<UniquePtr<UHPointLightComponent>> TestPointLights;
	std::vector<XMFLOAT3> TestPointLightOrigin;
	std::vector<UniquePtr<UHPointLightComponent>> TestPointLights2;
	std::vector<XMFLOAT3> TestPointLightOrigin2;

	std::vector<UniquePtr<UHSpotLightComponent>> TestSpotLights;
	std::vector<UniquePtr<UHSpotLightComponent>> TestSpotLights2;

	UHSkyLightComponent DefaultSkyLight;
	UHMeshRendererComponent* Geo364Renderer;

	XMFLOAT3 Geo364OriginPos;
	float TimeCounter;
	float TimeSign;
	UHDemoType TestType;
	float PointLightTimeCounter;
};