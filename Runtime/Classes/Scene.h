#pragma once
#include "Object.h"
#include "../Components/MeshRenderer.h"
#include "../Components/Camera.h"
#include "../Components/Light.h"
#include "../Components/SkyLight.h"
#include "../Engine/Graphic.h"
#include <vector>
#include <memory>
#include "TextureCube.h"

class UHAssetManager;
class UHGraphic;
class UHConfigManager;
class UHRawInput;
class UHGameTimer;

// scene class of UH engine
// for now, there is no "gameobject" or "actor" concept in UH
// all components are objects
class UHScene : public UHObject
{
public:
	UHScene();
	void Initialize(UHAssetManager* InAsset, UHGraphic* InGfx, UHConfigManager* InConfig, UHRawInput* InInput, UHGameTimer* InTimer);
	void Release();
	void Update();

#if WITH_DEBUG
	void ReassignRenderer(UHMeshRendererComponent* InRenderer);
#endif

	size_t GetAllRendererCount() const;
	size_t GetMaterialCount() const;
	size_t GetDirLightCount() const;
	size_t GetPointLightCount() const;

	std::vector<UHMeshRendererComponent*> GetAllRenderers() const;
	std::vector<UHMeshRendererComponent*> GetOpaqueRenderers() const;
	std::vector<UHMeshRendererComponent*> GetTranslucentRenderers() const;
	std::vector<UHDirectionalLightComponent*> GetDirLights() const;
	std::vector<UHPointLightComponent*> GetPointLights() const;
	std::vector<UHMaterial*> GetMaterials() const;
	UHCameraComponent* GetMainCamera();
	UHSkyLightComponent* GetSkyLight();
	UHMeshRendererComponent* GetSkyboxRenderer() const;

	UHMeshRendererComponent* AddMeshRenderer(UHMesh* InMesh, UHMaterial* InMaterial);
	void AddDirectionalLight(UHDirectionalLightComponent* InLight);
	void AddPointLight(UHPointLightComponent* InLight);
	void SetSkyLight(UHSkyLightComponent* InSkyLight);

private:
	void UpdateCamera();

	UHConfigManager* ConfigCache;
	UHRawInput* Input;
	UHGameTimer* Timer;

	// camera define
	// @TODO: better camera management
	UHCameraComponent DefaultCamera;
	UHSkyLightComponent* CurrentSkyLight;

	std::vector<UniquePtr<UHMeshRendererComponent>> MeshRenderers;
	std::vector<UHMaterial*> Materials;
	std::vector<UHMeshRendererComponent*> Renderers;
	std::vector<UHMeshRendererComponent*> OpaqueRenderers;
	std::vector<UHMeshRendererComponent*> TranslucentRenderers;
	std::vector<UHDirectionalLightComponent*> DirectionalLights;
	std::vector<UHPointLightComponent*> PointLights;
	UHMeshRendererComponent* SkyboxRenderer;
};