#include "Scene.h"
#include "../Engine/Graphic.h"
#include "../Engine/Asset.h"
#include "../Engine/Config.h"
#include "../Engine/Input.h"
#include "../Engine/GameTimer.h"
#include "../Engine/Engine.h"
#include "../Components/GameScript.h"

UHScene::UHScene()
	: ConfigCache(nullptr)
	, Input(nullptr)
	, Timer(nullptr)
	, CurrentSkyLight(nullptr)
#if WITH_EDITOR
	, CurrentSelectedComp(nullptr)
#endif
	, MainCamera(nullptr)
{
	SetName("Scene" + std::to_string(GetId()));
}

void UHScene::OnSave(std::ofstream& OutStream)
{
	UHObject::OnSave(OutStream);

	// save all components in a scene, note that this does not include game script.
	// since the game script is always registered in runtime for now.
	int32_t ComponentCount = 0;
	for (size_t Idx = 0; Idx < ComponentPools.size(); Idx++)
	{
		if (ComponentPools[Idx]->GetObjectClassId() != UHGameScript::ClassId)
		{
			ComponentCount++;
		}
	}

	// output number of components and component info after it
	// component info consists of the class id and their own data
	OutStream.write(reinterpret_cast<const char*>(&ComponentCount), sizeof(ComponentCount));
	for (size_t Idx = 0; Idx < ComponentPools.size(); Idx++)
	{
		const uint32_t ClassId = ComponentPools[Idx]->GetObjectClassId();
		if (ClassId != UHGameScript::ClassId)
		{
			OutStream.write(reinterpret_cast<const char*>(&ClassId), sizeof(ClassId));
			ComponentPools[Idx]->OnSave(OutStream);
		}
	}
}

void UHScene::OnLoad(std::ifstream& InStream)
{
	UHObject::OnLoad(InStream);

	int32_t ComponentCount;
	InStream.read(reinterpret_cast<char*>(&ComponentCount), sizeof(ComponentCount));

	// load and request components based on their type
	for (int32_t Idx = 0; Idx < ComponentCount; Idx++)
	{
		uint32_t ClassId;
		InStream.read(reinterpret_cast<char*>(&ClassId), sizeof(ClassId));

		// request component based on class id
		UHComponent* NewComp = RequestComponent(ClassId);
		NewComp->OnLoad(InStream);
	}
}

void UHScene::OnPostLoad(UHAssetManager* InAssetMgr)
{
	// certain types of component needs a post load callback to setup their reference
	for (UniquePtr<UHComponent>& Comp : ComponentPools)
	{
		Comp->OnPostLoad(InAssetMgr);
	}

	// force a update after post loading callback
	for (UniquePtr<UHComponent>& Comp : ComponentPools)
	{
		Comp->Update();
	}
}

void UHScene::Initialize(UHEngine* InEngine)
{
	ConfigCache = InEngine->GetConfigManager();
	Input = InEngine->GetRawInput();
	Timer = InEngine->GetGameTimer();

	// call all scene initialized code in scripts
	for (const auto& Script : UHGameScripts)
	{
		Script.second->OnSceneInitialized(this, InEngine->GetAssetManager(), InEngine->GetGfx());
	}

	// after initialization actions
	for (UniquePtr<UHComponent>& Component : ComponentPools)
	{
		// mesh renderer append is delayed because they need to wait both mesh and material to be loaded
		if (Component->GetObjectClassId() == UHMeshRendererComponent::ClassId)
		{
			AddMeshRenderer((UHMeshRendererComponent*)Component.get());
		}
	}

	RefreshRendererBufferDataIndex();
	CalculateSceneBound();
}

void UHScene::Release()
{
	// container clear
	Renderers.clear();
	Materials.clear();
	Renderers.clear();
	OpaqueRenderers.clear();
	TranslucentRenderers.clear();
	DirectionalLights.clear();
	PointLights.clear();
	SpotLights.clear();
}

void UHScene::Update()
{
	UHGameTimerScope Scope("SceneUpdate", false);
	UpdateCamera();

	// for objects won't update per-frame, conditionally call update, save ~0.2ms time for me
	for (UHMeshRendererComponent* Renderer : Renderers)
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

	for (UHPointLightComponent* Light : PointLights)
	{
		if (Light->IsWorldDirty())
		{
			Light->Update();
		}
	}

	for (UHSpotLightComponent* Light : SpotLights)
	{
		if (Light->IsWorldDirty())
		{
			Light->Update();
		}
	}
}

void UHScene::RefreshRendererBufferDataIndex()
{
	// assign buffer index for renderers, moveable objects first
	int32_t BufferIdx = 0;
	for (size_t Idx = 0; Idx < Renderers.size(); Idx++)
	{
		if (Renderers[Idx]->IsMoveable())
		{
			Renderers[Idx]->SetBufferDataIndex(BufferIdx++);
		}
	}

	for (size_t Idx = 0; Idx < Renderers.size(); Idx++)
	{
		if (!Renderers[Idx]->IsMoveable())
		{
			Renderers[Idx]->SetBufferDataIndex(BufferIdx++);
		}
	}
}

UHComponent* UHScene::RequestComponent(uint32_t InComponentClassId)
{
	UniquePtr<UHComponent> NewComp;
	switch (InComponentClassId)
	{
	case UHCameraComponent::ClassId:
		NewComp = MakeUnique<UHCameraComponent>();
		if (MainCamera == nullptr)
		{
			MainCamera = (UHCameraComponent*)NewComp.get();
		}
		break;

	case UHDirectionalLightComponent::ClassId:
		NewComp = MakeUnique<UHDirectionalLightComponent>();
		AddDirectionalLight((UHDirectionalLightComponent*)NewComp.get());
		break;

	case UHPointLightComponent::ClassId:
		NewComp = MakeUnique<UHPointLightComponent>();
		AddPointLight((UHPointLightComponent*)NewComp.get());
		break;

	case UHSpotLightComponent::ClassId:
		NewComp = MakeUnique<UHSpotLightComponent>();
		AddSpotLight((UHSpotLightComponent*)NewComp.get());
		break;

	case UHSkyLightComponent::ClassId:
		NewComp = MakeUnique<UHSkyLightComponent>();
		CurrentSkyLight = (UHSkyLightComponent*)NewComp.get();
		break;

	case UHMeshRendererComponent::ClassId:
		NewComp = MakeUnique<UHMeshRendererComponent>();
		break;

	default:
		break;
	};

	UHComponent* Result = NewComp.get();
	ComponentPools.push_back(std::move(NewComp));
	return Result;
}

#if WITH_EDITOR
void UHScene::ReassignRenderer(UHMeshRendererComponent* InRenderer)
{
	// this won't change total number of renderers, but just re-assign between OpaqueRenderers and TranslucentRenderers list
	int32_t DeleteIdx = UHUtilities::FindIndex(OpaqueRenderers, InRenderer);
	if (DeleteIdx != UHINDEXNONE)
	{
		UHUtilities::RemoveByIndex(OpaqueRenderers, DeleteIdx);
	}

	DeleteIdx = UHUtilities::FindIndex(TranslucentRenderers, InRenderer);
	if (DeleteIdx != UHINDEXNONE)
	{
		UHUtilities::RemoveByIndex(TranslucentRenderers, DeleteIdx);
	}

	const UHMaterial* Mat = InRenderer->GetMaterial();
	if (Mat == nullptr)
	{
		return;
	}

	if (Mat->IsOpaque())
	{
		OpaqueRenderers.push_back(InRenderer);
	}
	else
	{
		TranslucentRenderers.push_back(InRenderer);
	}
}

void UHScene::SetCurrentSelectedComponent(UHComponent* InComp)
{
	CurrentSelectedComp = InComp;
}

UHComponent* UHScene::GetCurrentSelectedComponent() const
{
	return CurrentSelectedComp;
}

#endif

void UHScene::AddMeshRenderer(UHMeshRendererComponent* InRenderer)
{
	if (InRenderer->GetMesh() == nullptr || InRenderer->GetMaterial() == nullptr)
	{
		UHE_LOG(L"Missing mesh or material in mesh renderer!\n");
	}

	if (UHUtilities::FindIndex(Renderers, InRenderer) != UHINDEXNONE)
	{
		// renderer already in scene, skip it
		return;
	}

	Renderers.push_back(InRenderer);

	// collect material as well, assign constant index for both newly added and already added cases
	UHMaterial* InMaterial = InRenderer->GetMaterial();
	int32_t ConstIdx = UHUtilities::FindIndex(Materials, InMaterial);
	if (ConstIdx == UHINDEXNONE)
	{
		InMaterial->SetBufferDataIndex(static_cast<int32_t>(Materials.size()));
		Materials.push_back(InMaterial);
	}
	else
	{
		InMaterial->SetBufferDataIndex(ConstIdx);
	}

	switch (InMaterial->GetBlendMode())
	{
	case UHBlendMode::Opaque:
	case UHBlendMode::Masked:
		OpaqueRenderers.push_back(Renderers.back());
		break;

	case UHBlendMode::TranditionalAlpha:
		TranslucentRenderers.push_back(Renderers.back());
		break;

	default:
		break;
	}
}

void UHScene::AddDirectionalLight(UHDirectionalLightComponent* InLight)
{
	InLight->SetBufferDataIndex(static_cast<int32_t>(DirectionalLights.size()));
	DirectionalLights.push_back(InLight);
}

void UHScene::AddPointLight(UHPointLightComponent* InLight)
{
	InLight->SetBufferDataIndex(static_cast<int32_t>(PointLights.size()));
	PointLights.push_back(InLight);
}

void UHScene::AddSpotLight(UHSpotLightComponent* InLight)
{
	InLight->SetBufferDataIndex(static_cast<int32_t>(SpotLights.size()));
	SpotLights.push_back(InLight);
}

const std::vector<UniquePtr<UHComponent>>& UHScene::GetAllCompoments()
{
	return ComponentPools;
}

size_t UHScene::GetAllRendererCount() const
{
	return Renderers.size();
}

size_t UHScene::GetMaterialCount() const
{
	return Materials.size();
}

size_t UHScene::GetDirLightCount() const
{
	return DirectionalLights.size();
}

size_t UHScene::GetPointLightCount() const
{
	return PointLights.size();
}

size_t UHScene::GetSpotLightCount() const
{
	return SpotLights.size();
}

const std::vector<UHMeshRendererComponent*>& UHScene::GetAllRenderers() const
{
	return Renderers;
}

const std::vector<UHMeshRendererComponent*>& UHScene::GetOpaqueRenderers() const
{
	return OpaqueRenderers;
}

const std::vector<UHMeshRendererComponent*>& UHScene::GetTranslucentRenderers() const
{
	return TranslucentRenderers;
}

const std::vector<UHDirectionalLightComponent*>& UHScene::GetDirLights() const
{
	return DirectionalLights;
}

const std::vector<UHPointLightComponent*>& UHScene::GetPointLights() const
{
	return PointLights;
}

const std::vector<UHSpotLightComponent*>& UHScene::GetSpotLights() const
{
	return SpotLights;
}

const std::vector<UHMaterial*>& UHScene::GetMaterials() const
{
	return Materials;
}

UHCameraComponent* UHScene::GetMainCamera()
{
	return MainCamera;
}

UHSkyLightComponent* UHScene::GetSkyLight() const
{
	return CurrentSkyLight;
}

void UHScene::UpdateCamera()
{
	if (MainCamera == nullptr)
	{
		return;
	}

	// set temporal stuffs
	MainCamera->SetResolution(ConfigCache->RenderingSetting().RenderWidth, ConfigCache->RenderingSetting().RenderHeight);
	MainCamera->SetUseJitter(ConfigCache->RenderingSetting().bTemporalAA);

	// set aspect ratio of default camera
	MainCamera->SetAspect(ConfigCache->RenderingSetting().RenderWidth / static_cast<float>(ConfigCache->RenderingSetting().RenderHeight));

	float DefaultCameraMoveSpeed = ConfigCache->EngineSetting().DefaultCameraMoveSpeed * Timer->GetDeltaTime();
	float MouseRotSpeed = ConfigCache->EngineSetting().MouseRotationSpeed * Timer->GetDeltaTime();

	if (Input->IsKeyHold(ConfigCache->EngineSetting().ForwardKey))
	{
		MainCamera->Translate(XMFLOAT3(0, 0, DefaultCameraMoveSpeed));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().BackKey))
	{
		MainCamera->Translate(XMFLOAT3(0, 0, -DefaultCameraMoveSpeed));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().LeftKey))
	{
		MainCamera->Translate(XMFLOAT3(-DefaultCameraMoveSpeed, 0, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().RightKey))
	{
		MainCamera->Translate(XMFLOAT3(DefaultCameraMoveSpeed, 0, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().UpKey))
	{
		MainCamera->Translate(XMFLOAT3(0, DefaultCameraMoveSpeed, 0));
	}

	if (Input->IsKeyHold(ConfigCache->EngineSetting().DownKey))
	{
		MainCamera->Translate(XMFLOAT3(0, -DefaultCameraMoveSpeed, 0));
	}

	if (Input->IsRightMouseHold())
	{
		float X = static_cast<float>(Input->GetMouseData().lLastX) * MouseRotSpeed;
		float Y = static_cast<float>(Input->GetMouseData().lLastY) * MouseRotSpeed;

		MainCamera->Rotate(XMFLOAT3(Y, 0, 0));
		MainCamera->Rotate(XMFLOAT3(0, X, 0), UHTransformSpace::World);
	}

	MainCamera->Update();
}

void UHScene::CalculateSceneBound()
{
	// calculate scene bound based on renderer bounds
	SceneBound = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0));
	for (const UHMeshRendererComponent* Renderer : Renderers)
	{
		const BoundingBox& RendererBound = Renderer->GetRendererBound();
		BoundingBox::CreateMerged(SceneBound, SceneBound, RendererBound);
	}
}