#pragma once
#include "../Runtime/Components/GameScript.h"
#include "../Runtime/Components/Light.h"
#include "../Runtime/Components/SkyLight.h"
#include "../Runtime/Components/MeshRenderer.h"

// UH demo script
class UHDemoScript : public UHGameScript
{
public:
	UHDemoScript();

	virtual void OnEngineUpdate(UHGameTimer* InGameTimer) override;
	virtual void OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx) override;

private:
	UHDirectionalLightComponent DefaultDirectionalLight;
	UHDirectionalLightComponent SecondDirectionalLight;
	UHSkyLightComponent DefaultSkyLight;
	UHMeshRendererComponent* Geo364Renderer;
	XMFLOAT3 Geo364OriginPos;
	float TimeCounter;
	float TimeSign;
};