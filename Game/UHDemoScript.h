#pragma once
#include "../Runtime/Components/GameScript.h"
#include "../Runtime/Components/Light.h"
#include "../Runtime/Components/SkyLight.h"
#include "../Runtime/Components/MeshRenderer.h"
#include "../Runtime/Components/Camera.h"

enum class UHDemoType
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

	virtual void OnEngineInitialized(UHEngine* InEngine) override;
	virtual void OnEngineUpdate(float DeltaTime) override;
	virtual void OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx) override;

private:
	void ObsoleteInitialization(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx);

	UHCameraComponent* DefaultCamera;
	UHDirectionalLightComponent* DefaultDirectionalLight;
	UHDirectionalLightComponent* SecondDirectionalLight;
	std::vector<UHPointLightComponent*> TestPointLights;
	std::vector<XMFLOAT3> TestPointLightOrigin;
	std::vector<UHPointLightComponent*> TestPointLights2;
	std::vector<XMFLOAT3> TestPointLightOrigin2;

	std::vector<UHSpotLightComponent*> TestSpotLights;
	std::vector<UHSpotLightComponent*> TestSpotLights2;

	UHSkyLightComponent* DefaultSkyLight;
	UHMeshRendererComponent* Geo364Renderer;
	UHMeshRendererComponent* Door3363Renderer;

	XMFLOAT3 Geo364OriginPos;
	float TimeCounter;
	float TimeSign;
	UHDemoType TestType;
	float PointLightTimeCounter;
	UHEngine* EngineCache;
};