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
class UHEngine;

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

	void Initialize(UHEngine* InEngine);
	void Release();
	void Update();

	void RefreshRendererBufferDataIndex();
	UHComponent* RequestComponent(uint32_t InComponentClassId);

#if WITH_EDITOR
	void ReassignRenderer(UHMeshRendererComponent* InRenderer);
	void SetCurrentSelectedComponent(UHComponent* InComp);
	UHComponent* GetCurrentSelectedComponent() const;
#endif
	const std::vector<UniquePtr<UHComponent>>& GetAllCompoments();

	size_t GetAllRendererCount() const;
	size_t GetMaterialCount() const;
	size_t GetDirLightCount() const;
	size_t GetPointLightCount() const;
	size_t GetSpotLightCount() const;

	const std::vector<UHMeshRendererComponent*>& GetAllRenderers() const;
	const std::vector<UHMeshRendererComponent*>& GetOpaqueRenderers() const;
	const std::vector<UHMeshRendererComponent*>& GetTranslucentRenderers() const;
	const std::vector<UHDirectionalLightComponent*>& GetDirLights() const;
	const std::vector<UHPointLightComponent*>& GetPointLights() const;
	const std::vector<UHSpotLightComponent*>& GetSpotLights() const;
	const std::vector<UHMaterial*>& GetMaterials() const;
	UHCameraComponent* GetMainCamera();
	UHSkyLightComponent* GetSkyLight() const;
	const BoundingBox& GetSceneBound() const;

	void AddMeshRenderer(UHMeshRendererComponent* InRenderer);
private:
	void AddDirectionalLight(UHDirectionalLightComponent* InLight);
	void AddPointLight(UHPointLightComponent* InLight);
	void AddSpotLight(UHSpotLightComponent* InLight);
	void UpdateCamera();
	void CalculateSceneBound();

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
	BoundingBox SceneBound;

#if WITH_EDITOR
	UHComponent* CurrentSelectedComp;
#endif
};