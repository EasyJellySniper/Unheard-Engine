#include "Scene.h"
#include "../Engine/Graphic.h"
#include "../Engine/Asset.h"
#include "../Engine/Config.h"
#include "../Engine/Input.h"
#include "../Engine/GameTimer.h"
#include "../Components/GameScript.h"

UHScene::UHScene()
	: SkyboxRenderer(nullptr)
	, ConfigCache(nullptr)
	, Input(nullptr)
	, Timer(nullptr)
	, CurrentSkyLight(nullptr)
{

}

void UHScene::Initialize(UHAssetManager* InAsset, UHGraphic* InGfx, UHConfigManager* InConfig, UHRawInput* InInput, UHGameTimer* InTimer)
{
	ConfigCache = InConfig;
	Input = InInput;
	Timer = InTimer;

	// call all scene initialized code in scripts
	for (const auto Script : UHGameScripts)
	{
		Script.second->OnSceneInitialized(this, InAsset, InGfx);
	}

	if (SkyboxRenderer)
	{
		SkyboxRenderer->SetScale(XMFLOAT3(GWorldMax, GWorldMax, GWorldMax));
	}
}

void UHScene::Release()
{
	for (auto& Renderer : MeshRenderers)
	{
		Renderer.reset();
	}

	// container clear
	MeshRenderers.clear();
	Materials.clear();
	Renderers.clear();
	OpaqueRenderers.clear();
	TranslucentRenderers.clear();
	DirectionalLights.clear();
}

void UHScene::Update()
{
	UpdateCamera();

	// make skybox always follow camera's pos
	if (SkyboxRenderer)
	{
		SkyboxRenderer->SetPosition(DefaultCamera.GetPosition());
		SkyboxRenderer->Update();
	}

	// for objects won't update per-frame, conditionally call update, save ~0.2ms time for me
	for (auto& Renderer : MeshRenderers)
	{
		if (Renderer->IsWorldDirty())
		{
			Renderer->Update();
		}
	}

	for (UHDirectionalLightComponent* Light : DirectionalLights)
	{
		if (Light->IsWorldDirty())
		{
			Light->Update();
		}
	}
}

#if WITH_DEBUG
void UHScene::ReassignRenderer(UHMeshRendererComponent* InRenderer)
{
	// this won't change total number of renderers, but just re-assign between OpaqueRenderers and TranslucentRenderers list
	int32_t DeleteIdx = UHUtilities::FindIndex(OpaqueRenderers, InRenderer);
	if (DeleteIdx != -1)
	{
		UHUtilities::RemoveByIndex(OpaqueRenderers, DeleteIdx);
	}

	DeleteIdx = UHUtilities::FindIndex(TranslucentRenderers, InRenderer);
	if (DeleteIdx != -1)
	{
		UHUtilities::RemoveByIndex(TranslucentRenderers, DeleteIdx);
	}

	const UHMaterial* Mat = InRenderer->GetMaterial();
	if (Mat == nullptr)
	{
		return;
	}

	if (Mat->GetBlendMode() < UHBlendMode::TranditionalAlpha)
	{
		OpaqueRenderers.push_back(InRenderer);
	}
	else
	{
		TranslucentRenderers.push_back(InRenderer);
	}
}
#endif

UHMeshRendererComponent* UHScene::AddMeshRenderer(UHMesh* InMesh, UHMaterial* InMaterial)
{
	if (InMesh == nullptr || InMaterial == nullptr)
	{
		UHE_LOG(L"Error when adding mesh renderer!\n");
		return nullptr;
	}

	std::unique_ptr<UHMeshRendererComponent> NewRenderer = std::make_unique<UHMeshRendererComponent>(InMesh, InMaterial);

	// set transform from imported info
	NewRenderer->SetPosition(InMesh->GetImportedTranslation());
	NewRenderer->SetRotation(InMesh->GetImportedRotation());
	NewRenderer->SetScale(InMesh->GetImportedScale());

	// give constant index, so the order in container doesn't matter when copying to constant buffer
	NewRenderer->SetBufferDataIndex(static_cast<int32_t>(MeshRenderers.size()));
	MeshRenderers.push_back(std::move(NewRenderer));

	// collect material as well, assign constant index for both newly added and already added cases
	int32_t ConstIdx = UHUtilities::FindIndex(Materials, InMaterial);
	if (ConstIdx == -1)
	{
		InMaterial->SetBufferDataIndex(static_cast<int32_t>(Materials.size()));
		Materials.push_back(InMaterial);
	}
	else
	{
		InMaterial->SetBufferDataIndex(ConstIdx);
	}

	// adds raw pointer cache to vectors
	Renderers.push_back(MeshRenderers.back().get());
	
	// cache skybox renderer if it's a sky material
	if (InMaterial->IsSkybox())
	{
		SkyboxRenderer = MeshRenderers.back().get();
	}
	else
	{
		// otherwise, adds cache based on blend mode
		switch (InMaterial->GetBlendMode())
		{
		case UHBlendMode::Opaque:
		case UHBlendMode::Masked:
			OpaqueRenderers.push_back(MeshRenderers.back().get());
			break;

		case UHBlendMode::TranditionalAlpha:
			TranslucentRenderers.push_back(MeshRenderers.back().get());
			break;

		default:
			break;
		}
	}

	// return added renderer, can use it or not
	return Renderers.back();
}

void UHScene::AddDirectionalLight(UHDirectionalLightComponent* InLight)
{
	InLight->SetBufferDataIndex(static_cast<int32_t>(DirectionalLights.size()));
	DirectionalLights.push_back(InLight);
}

void UHScene::SetSkyLight(UHSkyLightComponent* InSkyLight)
{
	CurrentSkyLight = InSkyLight;
}

size_t UHScene::GetAllRendererCount() const
{
	return MeshRenderers.size();
}

size_t UHScene::GetMaterialCount() const
{
	return Materials.size();
}

size_t UHScene::GetDirLightCount() const
{
	return DirectionalLights.size();
}

std::vector<UHMeshRendererComponent*> UHScene::GetAllRenderers() const
{
	return Renderers;
}

std::vector<UHMeshRendererComponent*> UHScene::GetOpaqueRenderers() const
{
	return OpaqueRenderers;
}

std::vector<UHMeshRendererComponent*> UHScene::GetTranslucentRenderers() const
{
	return TranslucentRenderers;
}

std::vector<UHDirectionalLightComponent*> UHScene::GetDirLights() const
{
	return DirectionalLights;
}

std::vector<UHMaterial*> UHScene::GetMaterials() const
{
	return Materials;
}

UHCameraComponent* UHScene::GetMainCamera()
{
	return &DefaultCamera;
}

UHSkyLightComponent* UHScene::GetSkyLight()
{
	return CurrentSkyLight;
}

UHMeshRendererComponent* UHScene::GetSkyboxRenderer() const
{
	return SkyboxRenderer;
}

void UHScene::UpdateCamera()
{
	// set temporal stuffs
	DefaultCamera.SetResolution(ConfigCache->RenderingSetting().RenderWidth, ConfigCache->RenderingSetting().RenderHeight);
	DefaultCamera.SetUseJitter(ConfigCache->RenderingSetting().bTemporalAA);

	// set aspect ratio of default camera
	DefaultCamera.SetAspect(ConfigCache->RenderingSetting().RenderWidth / static_cast<float>(ConfigCache->RenderingSetting().RenderHeight));

	float DefaultCameraMoveSpeed = ConfigCache->EngineSetting().DefaultCameraMoveSpeed * Timer->GetDeltaTime();
	float MouseRotSpeed = ConfigCache->EngineSetting().MouseRotationSpeed * Timer->GetDeltaTime();

	if (Input->IsKeyHold(ConfigCache->EngineSetting().ForwardKey))
	{
		DefaultCamera.Translate(XMFLOAT3(0, 0, DefaultCameraMoveSpeed));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().BackKey))
	{
		DefaultCamera.Translate(XMFLOAT3(0, 0, -DefaultCameraMoveSpeed));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().LeftKey))
	{
		DefaultCamera.Translate(XMFLOAT3(-DefaultCameraMoveSpeed, 0, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().RightKey))
	{
		DefaultCamera.Translate(XMFLOAT3(DefaultCameraMoveSpeed, 0, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().UpKey))
	{
		DefaultCamera.Translate(XMFLOAT3(0, DefaultCameraMoveSpeed, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().DownKey))
	{
		DefaultCamera.Translate(XMFLOAT3(0, -DefaultCameraMoveSpeed, 0));
	}

	if (Input->IsRightMouseHold())
	{
		float X = static_cast<float>(Input->GetMouseData().lLastX) * MouseRotSpeed;
		float Y = static_cast<float>(Input->GetMouseData().lLastY) * MouseRotSpeed;

		DefaultCamera.Rotate(XMFLOAT3(Y, 0, 0));
		DefaultCamera.Rotate(XMFLOAT3(0, X, 0), UHTransformSpace::World);
	}

	DefaultCamera.Update();
}