#include "UHDemoScript.h"
#include "../Runtime/Classes/Scene.h"
#include "../Runtime/Engine/GameTimer.h"

UHDemoScript DemoScript;

UHDemoScript::UHDemoScript()
	: Geo364Renderer(nullptr)
	, Geo364OriginPos(XMFLOAT3())
	, TimeCounter(0)
	, TimeSign(1)
{

}

void UHDemoScript::OnEngineUpdate(UHGameTimer* InGameTimer)
{
	float LightRotSpd = 5.0f * InGameTimer->GetDeltaTime();
	DefaultDirectionalLight.Rotate(XMFLOAT3(0, LightRotSpd, 0), UHTransformSpace::World);

	// move geo364
	if (Geo364Renderer)
	{
		float MoveTime = 3.0f;
		XMFLOAT3 Pos1 = Geo364OriginPos + XMFLOAT3(-1, 0, 0);
		XMFLOAT3 Pos2 = Geo364OriginPos + XMFLOAT3(1, 0, 0);
		XMFLOAT3 NewPos = MathHelpers::LerpVector(Pos1, Pos2, TimeCounter / MoveTime);

		TimeCounter += InGameTimer->GetDeltaTime() * TimeSign;
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
}

void UHDemoScript::OnSceneInitialized(UHScene* InScene, UHAssetManager* InAsset, UHGraphic* InGfx)
{
	// request a texture cube, put slices in a order as: +x,-x,+y,-y,+z,-z
	std::vector<UHTexture2D*> Slices = { InAsset->GetTexture2DByPath("Daylight Box_Pieces/posx")
		,InAsset->GetTexture2DByPath("Daylight Box_Pieces/negx")
		,InAsset->GetTexture2DByPath("Daylight Box_Pieces/posy")
		,InAsset->GetTexture2DByPath("Daylight Box_Pieces/negy")
		,InAsset->GetTexture2DByPath("Daylight Box_Pieces/posz")
		,InAsset->GetTexture2DByPath("Daylight Box_Pieces/negz") };

	UHTextureCube* SkyCubeTex = InGfx->RequestTextureCube("UHDefaultSkyCubeTex", Slices);
	UHSampler* SkyCubeSampler = InGfx->RequestTextureSampler(UHSamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT
		, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, static_cast<float>(Slices[0]->GetMipMapCount())));

	// brutal test for renderers
	float MarginX = 75.0f;
	float MarginZ = 25.0f;
	float OffsetX[] = { 0, -1,0,1,-1,1,-1,0,1 };
	float OffsetZ[] = { 0, -1,-1,-1,0,0,1,1,1 };

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

			// set env cube
			Mat->SetSystemTex(UHSystemTextureType::SkyCube, SkyCubeTex);
			Mat->SetSystemSampler(UHSystemTextureType::SkyCube, SkyCubeSampler);

			UHMeshRendererComponent* NewRenderer = InScene->AddMeshRenderer(LoadedMeshes[Idx], Mat);
			if (LoadedMeshes[Idx]->GetName() == "Geo364" && Offset == 0)
			{
				Geo364Renderer = NewRenderer;
				Geo364OriginPos = Geo364Renderer->GetPosition();
			}

			NewRenderer->Translate(XMFLOAT3(MarginX * OffsetX[Offset], 0, MarginZ * OffsetZ[Offset]), UHTransformSpace::World);
		}
	}

	// add skybox renderer
	UHMesh* SkyCube = InAsset->GetMesh("UHMesh_Cube");
	UHMaterial* SkyMat = InGfx->RequestMaterial();
	SkyMat->SetCullMode(VK_CULL_MODE_FRONT_BIT);
	SkyMat->SetIsSkybox(true);
	SkyMat->SetName("UHDefaultSky");
	SkyMat->SetSystemTex(UHSystemTextureType::SkyCube, SkyCubeTex);
	SkyMat->SetSystemSampler(UHSystemTextureType::SkyCube, SkyCubeSampler);

	InScene->AddMeshRenderer(SkyCube, SkyMat);

	// setup default light
	DefaultDirectionalLight.SetLightColor(XMFLOAT4(0.95f, 0.91f, 0.6f, 0.4f));
	DefaultDirectionalLight.SetIntensity(2.5f);
	DefaultDirectionalLight.SetRotation(XMFLOAT3(45, 150, 0));
	InScene->AddDirectionalLight(&DefaultDirectionalLight);

	// setup default sky light
	DefaultSkyLight.SetSkyColor(XMFLOAT3(0.8f, 0.8f, 0.8f));
	DefaultSkyLight.SetGroundColor(XMFLOAT3(0.3f, 0.3f, 0.3f));
	DefaultSkyLight.SetSkyIntensity(1.0f);
	DefaultSkyLight.SetGroundIntensity(1.5f);
	InScene->SetSkyLight(&DefaultSkyLight);

	// setup default camera
	//InScene->GetMainCamera()->SetPosition(XMFLOAT3(0, 2, -15));
	InScene->GetMainCamera()->SetPosition(XMFLOAT3(138, 9, -25));
	InScene->GetMainCamera()->SetRotation(XMFLOAT3(0, -70, 0));
	InScene->GetMainCamera()->SetCullingDistance(1000.0f);

	// secondary light test
	SecondDirectionalLight.SetLightColor(XMFLOAT4(1.0f, 0.55f, 0.0f, 0.7f));
	SecondDirectionalLight.SetIntensity(2.5f);
	SecondDirectionalLight.SetRotation(XMFLOAT3(45, -30, 0));
	InScene->AddDirectionalLight(&SecondDirectionalLight);
}