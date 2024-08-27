#include "UHDemoScript.h"
#include "../Runtime/Classes/Scene.h"
#include "../Runtime/Engine/GameTimer.h"
#include "../Runtime/Engine/Engine.h"
#include <cmath>

UHDemoScript DemoScript;

UHDemoScript::UHDemoScript()
	: Geo364Renderer(nullptr)
	, Geo364OriginPos(XMFLOAT3())
	, TimeCounter(0)
	, TimeSign(1)
	, TestType(UHDemoType::DayTest)
	, PointLightTimeCounter(0.0f)
	, DefaultCamera(nullptr)
	, DefaultDirectionalLight(nullptr)
	, SecondDirectionalLight(nullptr)
	, DefaultSkyLight(nullptr)
{
	SetName("Demo Script Component");
}

void UHDemoScript::OnEngineInitialized(UHEngine* InEngine)
{
	if (TestType == UHDemoType::DayTest)
	{
		InEngine->OnLoadScene("Assets/Scenes/VikingWithStones.uhscene");
	}
	else if (TestType == UHDemoType::PointLightNight)
	{
		InEngine->OnLoadScene("Assets/Scenes/VikingHouses_PointLightNight.uhscene");
	}
	else
	{
		InEngine->OnLoadScene("Assets/Scenes/VikingHouses_SpotLightNight.uhscene");
	}

	//ObsoleteInitialization(InScene, InAsset, InGfx);
	//return;
}

void UHDemoScript::OnEngineUpdate(float DeltaTime)
{
	float LightRotSpd = 5.0f * DeltaTime;
	if (TestType == UHDemoType::DayTest)
	{
		//DefaultDirectionalLight.Rotate(XMFLOAT3(0, LightRotSpd, 0), UHTransformSpace::World);
	}

	// move geo364
	if (Geo364Renderer)
	{
		float MoveTime = 3.0f;
		XMFLOAT3 Pos1 = Geo364OriginPos + XMFLOAT3(-1, 0, 0);
		XMFLOAT3 Pos2 = Geo364OriginPos + XMFLOAT3(1, 0, 0);
		XMFLOAT3 NewPos = MathHelpers::LerpVector(Pos1, Pos2, TimeCounter / MoveTime);

		TimeCounter += DeltaTime * TimeSign;
		if (TimeCounter >= MoveTime)
		{
			TimeCounter = MoveTime;
			TimeSign = -1;
		}
		else if (TimeCounter < 0)
		{
			TimeCounter = 0;
			TimeSign = 1;
		}

		Geo364Renderer->SetPosition(NewPos);
	}

	// giving point light a little transform so it lits objects like a torch
	if (TestType == UHDemoType::PointLightNight)
	{
		float MaxRadius = 15.0f;
		for (int32_t Idx = 0; Idx < TestPointLights.size(); Idx++)
		{
			float NewIntensity = MathHelpers::Lerp(4.0f, 3.0f, PointLightTimeCounter);
			TestPointLights[Idx]->SetIntensity(NewIntensity);
			//TestPointLights2[Idx]->SetIntensity(NewIntensity);

			XMFLOAT3 Pos = TestPointLightOrigin[Idx];
			Pos = MathHelpers::LerpVector(Pos, Pos + XMFLOAT3(0, -0.05f, 0), PointLightTimeCounter);
			TestPointLights[Idx]->SetPosition(Pos);

			//Pos = TestPointLightOrigin2[Idx];
			//Pos = MathHelpers::LerpVector(Pos, Pos + XMFLOAT3(0, -0.05f, 0), PointLightTimeCounter);
			//TestPointLights2[Idx]->SetPosition(Pos);
		}

		PointLightTimeCounter += DeltaTime;
		if (PointLightTimeCounter > 0.35f)
		{
			PointLightTimeCounter = 0.0f;
		}
	}

	if (TestType == UHDemoType::SpotLightNight)
	{
		LightRotSpd = 45.0f * DeltaTime;
		for (UHSpotLightComponent* SpotLight : TestSpotLights)
		{
			SpotLight->Rotate(XMFLOAT3(0, LightRotSpd, 0), UHTransformSpace::World);
		}

		for (UHSpotLightComponent* SpotLight : TestSpotLights2)
		{
			SpotLight->Rotate(XMFLOAT3(0, LightRotSpd, 0), UHTransformSpace::World);
		}
	}
}

void UHDemoScript::OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx)
{
	const std::vector<UniquePtr<UHComponent>>& SceneComponents = InScene->GetAllCompoments();
	TestType = UHDemoType::DayTest;

	if (UHUtilities::StringFind(InScene->GetName(), "SpotLightNight"))
	{
		TestSpotLights.clear();
		for (const UniquePtr<UHComponent>& Comp : SceneComponents)
		{
			if (Comp->GetObjectClassId() == UHSpotLightComponent::ClassId)
			{
				TestSpotLights.push_back((UHSpotLightComponent*)Comp.get());
			}
		}
		TestType = UHDemoType::SpotLightNight;
	}

	if (UHUtilities::StringFind(InScene->GetName(), "PointLightNight"))
	{
		TestPointLights.clear();
		TestPointLightOrigin.clear();
		for (const UniquePtr<UHComponent>& Comp : SceneComponents)
		{
			if (Comp->GetObjectClassId() == UHPointLightComponent::ClassId)
			{
				TestPointLights.push_back((UHPointLightComponent*)Comp.get());
				TestPointLightOrigin.push_back(((UHPointLightComponent*)Comp.get())->GetPosition());
			}
		}
		TestType = UHDemoType::PointLightNight;
	}

	for (const UniquePtr<UHComponent>& Comp : SceneComponents)
	{
		if (Comp->GetObjectClassId() == UHMeshRendererComponent::ClassId)
		{
			UHMeshRendererComponent* MRC = (UHMeshRendererComponent*)Comp.get();
			if (UHUtilities::StringFind(MRC->GetName(), "1893"))
			{
				Geo364Renderer = MRC;
			}
		}
	}
}

// not used at the moment
void UHDemoScript::ObsoleteInitialization(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx)
{
	DefaultCamera = (UHCameraComponent*)InScene->RequestComponent(UHCameraComponent::ClassId);

	// brutal test for renderers
	float MarginX = 75.0f;
	float MarginZ = 25.0f;
	float OffsetX[] = { 0, -1,0,1,-1,1,-1,0,1 };
	float OffsetZ[] = { 0, -1,-1,-1,0,0,1,1,1 };

	// setup default light
	DefaultDirectionalLight = (UHDirectionalLightComponent*)InScene->RequestComponent(UHDirectionalLightComponent::ClassId);
	DefaultDirectionalLight->SetLightColor(XMFLOAT3(0.95f, 0.91f, 0.6f));
	DefaultDirectionalLight->SetIntensity(TestType != UHDemoType::DayTest ? 0.2f : 4.5f);
	DefaultDirectionalLight->SetRotation(XMFLOAT3(45, -120, 0));

	// setup default sky light
	DefaultSkyLight = (UHSkyLightComponent*)InScene->RequestComponent(UHSkyLightComponent::ClassId);
	DefaultSkyLight->SetSkyColor(XMFLOAT3(0.8f, 0.8f, 0.8f));
	DefaultSkyLight->SetGroundColor(XMFLOAT3(0.3f, 0.3f, 0.3f));
	DefaultSkyLight->SetSkyIntensity(TestType != UHDemoType::DayTest ? 0.15f : 2.0f);
	DefaultSkyLight->SetGroundIntensity(TestType != UHDemoType::DayTest ? 0.5f : 1.5f);

	if (TestType == UHDemoType::DayTest)
	{
		DefaultSkyLight->SetCubemap(InAsset->GetCubemapByName("table_mountain_1_puresky_4k_Cube"));
	}
	else
	{
		DefaultSkyLight->SetCubemap(InAsset->GetCubemapByName("kloppenheim_07_puresky_4k_Cube"));
	}

	// setup default camera
	InScene->GetMainCamera()->SetPosition(XMFLOAT3(0, 2, -15));
	InScene->GetMainCamera()->SetPosition(XMFLOAT3(138, 9, -25));
	InScene->GetMainCamera()->SetRotation(XMFLOAT3(0, -70, 0));
	InScene->GetMainCamera()->SetCullingDistance(1000.0f);

	// secondary light test
	SecondDirectionalLight = (UHDirectionalLightComponent*)InScene->RequestComponent(UHDirectionalLightComponent::ClassId);
	SecondDirectionalLight->SetLightColor(XMFLOAT3(1.0f, 0.55f, 0.0f));
	SecondDirectionalLight->SetIntensity(2.5f);
	SecondDirectionalLight->SetRotation(XMFLOAT3(45, -30, 0));
	SecondDirectionalLight->SetIsEnabled(false);

	// add mesh renderer for all loaded meshes
	std::vector<UHMesh*> LoadedMeshes = InAsset->GetUHMeshes();

	for (int32_t Offset = 0; Offset < sizeof(OffsetX) / sizeof(float); Offset++)
	{
		for (size_t Idx = 0; Idx < LoadedMeshes.size(); Idx++)
		{
			UHMaterial* Mat = InAsset->GetMaterial(LoadedMeshes[Idx]->GetImportedMaterialName());
			if (Mat == nullptr)
			{
				continue;
			}

			UHMeshRendererComponent* NewRenderer = (UHMeshRendererComponent*)InScene->RequestComponent(UHMeshRendererComponent::ClassId);
			if (LoadedMeshes[Idx]->GetName() == "Geo364" && Offset == 0)
			{
				Geo364Renderer = NewRenderer;
				Geo364OriginPos = Geo364Renderer->GetPosition();
			}

			NewRenderer->SetMesh(LoadedMeshes[Idx]);
			NewRenderer->SetMaterial(Mat);
			NewRenderer->SetPosition(LoadedMeshes[Idx]->GetImportedTranslation());
			NewRenderer->SetRotation(LoadedMeshes[Idx]->GetImportedRotation());
			NewRenderer->SetScale(LoadedMeshes[Idx]->GetImportedScale());
			NewRenderer->Translate(XMFLOAT3(MarginX * OffsetX[Offset], 0, MarginZ * OffsetZ[Offset]), UHTransformSpace::World);
		}
	}

	if (TestType == UHDemoType::PointLightNight)
	{
		// point light test
		float PointLightStartX = -105;
		float PointLightStartZ = -25;
		float MinColor = 0.2f;

		TestPointLights.resize(27);
		TestPointLightOrigin.resize(27);
		for (int32_t Idx = 0; Idx < 3; Idx++)
		{
			for (int32_t Jdx = 0; Jdx < 9; Jdx++)
			{
				const int32_t LightIndex = Idx * 9 + Jdx;
				TestPointLights[LightIndex] = (UHPointLightComponent*)InScene->RequestComponent(UHPointLightComponent::ClassId);
				TestPointLights[LightIndex]->SetIntensity(1.5f);
				TestPointLights[LightIndex]->SetRadius(10.0f);
				TestPointLights[LightIndex]->SetLightColor(XMFLOAT3(MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor));
				TestPointLights[LightIndex]->SetPosition(XMFLOAT3(PointLightStartX + Jdx * 25.0f, 1.0f, PointLightStartZ + Idx * 25.0f));
				TestPointLightOrigin[LightIndex] = TestPointLights[LightIndex]->GetPosition();
			}
		}

		// point light test - the 2nd group
		PointLightStartX = -95;
		PointLightStartZ = -25;
		TestPointLights2.resize(27);
		TestPointLightOrigin2.resize(27);
		for (int32_t Idx = 0; Idx < 3; Idx++)
		{
			for (int32_t Jdx = 0; Jdx < 9; Jdx++)
			{
				const int32_t LightIndex = Idx * 9 + Jdx;
				TestPointLights2[LightIndex] = (UHPointLightComponent*)InScene->RequestComponent(UHPointLightComponent::ClassId);
				TestPointLights2[LightIndex]->SetIntensity(1.5f);
				TestPointLights2[LightIndex]->SetRadius(10.0f);
				TestPointLights2[LightIndex]->SetLightColor(XMFLOAT3(MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor));
				TestPointLights2[LightIndex]->SetPosition(XMFLOAT3(PointLightStartX + Jdx * 25.0f, 1.0f, PointLightStartZ + Idx * 25.0f));
				TestPointLightOrigin2[LightIndex] = TestPointLights2[LightIndex]->GetPosition();
			}
		}
	}

	if (TestType == UHDemoType::SpotLightNight)
	{
		// spot light test
		float SpotLightStartX = -105;
		float SpotLightStartZ = -25;
		float MinColor = 0.2f;

		TestSpotLights.resize(27);
		for (int32_t Idx = 0; Idx < 3; Idx++)
		{
			for (int32_t Jdx = 0; Jdx < 9; Jdx++)
			{
				const int32_t LightIndex = Idx * 9 + Jdx;
				TestSpotLights[LightIndex] = (UHSpotLightComponent*)InScene->RequestComponent(UHSpotLightComponent::ClassId);
				TestSpotLights[LightIndex]->SetIntensity(4.0f);
				TestSpotLights[LightIndex]->SetRadius(20.0f);
				TestSpotLights[LightIndex]->SetAngle(MathHelpers::RandFloat() * 30.0f + 30.0f);
				TestSpotLights[LightIndex]->SetLightColor(XMFLOAT3(MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor));
				TestSpotLights[LightIndex]->SetPosition(XMFLOAT3(SpotLightStartX + Jdx * 25.0f, 5.0f, SpotLightStartZ + Idx * 25.0f));

				const float RandX = MathHelpers::Lerp(-30.0f, 30.0f, MathHelpers::RandFloat());
				TestSpotLights[LightIndex]->SetRotation(XMFLOAT3(90.0f + RandX, 0.0f, 0.0f));
			}
		}

		// spot light test - 2nd group
		SpotLightStartX = -95;
		SpotLightStartZ = -25;

		TestSpotLights2.resize(27);
		for (int32_t Idx = 0; Idx < 3; Idx++)
		{
			for (int32_t Jdx = 0; Jdx < 9; Jdx++)
			{
				const int32_t LightIndex = Idx * 9 + Jdx;
				TestSpotLights2[LightIndex] = (UHSpotLightComponent*)InScene->RequestComponent(UHSpotLightComponent::ClassId);
				TestSpotLights2[LightIndex]->SetIntensity(4.0f);
				TestSpotLights2[LightIndex]->SetRadius(20.0f);
				TestSpotLights2[LightIndex]->SetAngle(MathHelpers::RandFloat() * 30.0f + 30.0f);
				TestSpotLights2[LightIndex]->SetLightColor(XMFLOAT3(MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor, MathHelpers::RandFloat() + MinColor));
				TestSpotLights2[LightIndex]->SetPosition(XMFLOAT3(SpotLightStartX + Jdx * 25.0f, 5.0f, SpotLightStartZ + Idx * 25.0f));

				const float RandX = MathHelpers::Lerp(-30.0f, 30.0f, MathHelpers::RandFloat());
				TestSpotLights2[LightIndex]->SetRotation(XMFLOAT3(90.0f + RandX, 0.0f, 0.0f));
			}
		}
	}
}