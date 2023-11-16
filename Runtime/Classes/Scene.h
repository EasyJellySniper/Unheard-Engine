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
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;
	virtual void OnPostLoad(UHAssetManager* InAssetMgr) override;

	void Initialize(UHAssetManager* InAsset, UHGraphic* InGfx, UHConfigManager* InConfig, UHRawInput* InInput, UHGameTimer* InTimer);
	void Release();
	void Update();

	UHComponent* RequestComponent(uint32_t InComponentClassId);

#if WITH_EDITOR
	void ReassignRenderer(UHMeshRendererComponent* InRenderer);
	void SetCurrentSelectedComponent(UHComponent* InComp);
	UHComponent* GetCurrentSelectedComponent() const;
#endif
	std::vector<UniquePtr<UHComponent>>& GetAllCompoments();

	size_t GetAllRendererCount() const;
	size_t GetMaterialCount() const;
	size_t GetDirLightCount() const;
	size_t GetPointLightCount() const;
	size_t GetSpotLightCount() const;

	std::vector<UHMeshRendererComponent*> GetAllRenderers() const;
	std::vector<UHMeshRendererComponent*> GetOpaqueRenderers() const;
	std::vector<UHMeshRendererComponent*> GetTranslucentRenderers() const;
	std::vector<UHDirectionalLightComponent*> GetDirLights() const;
	std::vector<UHPointLightComponent*> GetPointLights() const;
	std::vector<UHSpotLightComponent*> GetSpotLights() const;
	std::vector<UHMaterial*> GetMaterials() const;
	UHCameraComponent* GetMainCamera();
	UHSkyLightComponent* GetSkyLight() const;

private:
	void AddDirectionalLight(UHDirectionalLightComponent* InLight);
	void AddPointLight(UHPointLightComponent* InLight);
	void AddSpotLight(UHSpotLightComponent* InLight);
	void AddMeshRenderer(UHMeshRendererComponent* InRenderer);
	void UpdateCamera();

	UHConfigManager* ConfigCache;
	UHRawInput* Input;
	UHGameTimer* Timer;
	UHCameraComponent* MainCamera;
	UHSkyLightComponent* CurrentSkyLight;

	std::vector<UHMaterial*> Materials;
	std::vector<UHMeshRendererComponent*> Renderers;
	std::vector<UHMeshRendererComponent*> OpaqueRenderers;
	std::vector<UHMeshRendererComponent*> TranslucentRenderers;
	std::vector<UHDirectionalLightComponent*> DirectionalLights;
	std::vector<UHPointLightComponent*> PointLights;
	std::vector<UHSpotLightComponent*> SpotLights;

	std::vector<UniquePtr<UHComponent>> ComponentPools;

#if WITH_EDITOR
	UHComponent* CurrentSelectedComp;
#endif
};