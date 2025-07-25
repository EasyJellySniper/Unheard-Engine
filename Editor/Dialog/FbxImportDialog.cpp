#include "FbxImportDialog.h"

#if WITH_EDITOR
#include "Runtime/Engine/Engine.h"
#include "../Classes/EditorUtils.h"
#include "Runtime/Classes/AssetPath.h"
#include "../../Editor/Classes/FbxImporter.h"
#include "../../Editor/Editor/Editor.h"

UHFbxImportDialog::UHFbxImportDialog(UHEngine* InEngine)
	: UHDialog(nullptr, nullptr)
	, Engine(InEngine)
	, bCreateSceneObjectAfterImport(false)
	, bCreateNewScene(false)
{
	FBXImporterInterface = MakeUnique<UHFbxImporter>();
}

UHFbxImportDialog::~UHFbxImportDialog()
{
	FBXImporterInterface.reset();
}

void UHFbxImportDialog::ShowDialog()
{
	if (bIsOpened)
	{
		return;
	}

	UHDialog::ShowDialog();
	InputSourceFile = "";
	MeshOutputPath = "";
	MaterialOutputPath = "";
	CurrConvertUnitText = "meter";
}

void UHFbxImportDialog::Update(bool& bIsDialogActive)
{
	if (!bIsOpened)
	{
		return;
	}

	// fbx importing
	ImGui::Begin("Fbx import", &bIsOpened);
	ImGui::Text("Fbx import>>>>>>>>>>");
	if (ImGui::BeginTable("Fbx Creation", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableNextColumn();
		ImGui::Checkbox("Create scene objects after import", &bCreateSceneObjectAfterImport);
		if (bCreateSceneObjectAfterImport)
		{
			static int32_t RadioIdx = 0;
			ImGui::RadioButton("Create new scene", &RadioIdx, 0);
			ImGui::RadioButton("Add to current scene", &RadioIdx, 1);
			bCreateNewScene = RadioIdx == 0 ? true : false;
		}

		// convert unit selection
		const std::string ConvertUnitTexts[] = { "meter","centimeter" };
		if (ImGui::BeginCombo("Convert Unit", CurrConvertUnitText.c_str()))
		{
			for (int32_t Idx = 0; Idx < 2; Idx++)
			{
				const bool bIsSelected = CurrConvertUnitText == ConvertUnitTexts[Idx];
				if (ImGui::Selectable(ConvertUnitTexts[Idx].c_str(), bIsSelected))
				{
					CurrConvertUnitText = ConvertUnitTexts[Idx];
					FBXImporterInterface->SetConvertUnit((UHFbxConvertUnit)Idx);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::NewLine();
		if (ImGui::Button("Import"))
		{
			OnImport();
		}

		ImGui::TableNextColumn();
		ImGui::Text("Input File:");
		ImGui::Text(InputSourceFile.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Browse"))
		{
			std::filesystem::path DefaultPath = UHUtilities::ToStringW(GRawMeshAssetPath);
			DefaultPath = std::filesystem::absolute(DefaultPath);
			InputSourceFile = UHUtilities::ToStringA(UHEditorUtil::FileSelectInput({ {L"FBX Formats"}, { L"*.fbx"} }
				, DefaultPath.wstring()));
		}

		ImGui::Text("Mesh Output Path:");
		ImGui::Text(MeshOutputPath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Browse Mesh Output"))
		{
			std::filesystem::path DefaultPath = UHUtilities::ToStringW(GMeshAssetFolder);
			DefaultPath = std::filesystem::absolute(DefaultPath);
			MeshOutputPath = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder(DefaultPath.wstring()));
		}

		ImGui::Text("Material Output Path:");
		ImGui::Text(MaterialOutputPath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Browse Material Output"))
		{
			std::filesystem::path DefaultPath = UHUtilities::ToStringW(GMaterialAssetPath);
			DefaultPath = std::filesystem::absolute(DefaultPath);
			MaterialOutputPath = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder(DefaultPath.wstring()));
		}

		ImGui::Text("Texture Reference Path:");
		ImGui::Text(TextureReferencePath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Browse Texture Reference"))
		{
			std::filesystem::path DefaultPath = UHUtilities::ToStringW(GTextureAssetFolder);
			DefaultPath = std::filesystem::absolute(DefaultPath);
			TextureReferencePath = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder(DefaultPath.wstring()));
		}

		ImGui::EndTable();
	}
	ImGui::End();
}

bool IsPathValid(const std::filesystem::path& OutputFolder, std::filesystem::path AssetPath)
{
	AssetPath = std::filesystem::absolute(AssetPath);

	bool bIsValidOutputFolder = false;
	bIsValidOutputFolder |= UHUtilities::StringFind(OutputFolder.string() + "\\", AssetPath.string());
	bIsValidOutputFolder |= UHUtilities::StringFind(AssetPath.string(), OutputFolder.string() + "\\");
	return bIsValidOutputFolder && std::filesystem::exists(OutputFolder);
}

void UHFbxImportDialog::OnImport()
{
	const std::filesystem::path InputSource = InputSourceFile;
	if (!std::filesystem::exists(InputSource))
	{
		MessageBoxA(nullptr, "Invalid input source!", "Fbx Import", MB_OK);
		return;
	}

	if (!IsPathValid(MeshOutputPath, GMeshAssetFolder))
	{
		MessageBoxA(nullptr, "Invalid mesh output folder or it's not under the engine path Assets/Meshes!", "Fbx Import", MB_OK);
		return;
	}

	if (!IsPathValid(MaterialOutputPath, GMaterialAssetPath))
	{
		MessageBoxA(nullptr, "Invalid output folder or it's not under the engine path Assets/Materials!", "Fbx Import", MB_OK);
		return;
	}

	if (!IsPathValid(TextureReferencePath, GTextureAssetFolder))
	{
		MessageBoxA(nullptr, "Invalid texture reference folder or it's not under the engine path Assets/Textures!", "Fbx Import", MB_OK);
		return;
	}

	// import fbx
	FBXImporterInterface->SetMaterialOutputPath(MaterialOutputPath);
	UHFbxImportedData ImportedData = FBXImporterInterface->ImportRawFbx(InputSourceFile, TextureReferencePath);

	// Add imported meshes/materials to system, if the assets are already there, do not export them.
	// @TODO: consider a force overwriting option in the future
	std::vector<UHMesh*> MeshCache;
	UHAssetManager* AssetMgr = Engine->GetAssetManager();

	for (UniquePtr<UHMesh>& ImportMesh : ImportedData.ImportedMesh)
	{
		std::filesystem::path OutPath = MeshOutputPath + "/" + ImportMesh->GetName() + GMeshAssetExtension;

		// setup source path with ./ prefix removed
		std::filesystem::path SourcePath = std::filesystem::relative(MeshOutputPath, GMeshAssetFolder).string() + "/" + ImportMesh->GetName();
		ImportMesh->SetSourcePath(UHUtilities::StringReplace(SourcePath.string(), "./", ""));
		MeshCache.push_back(ImportMesh.get());

		if (!std::filesystem::exists(OutPath))
		{
			ImportMesh->Export(OutPath);
			AssetMgr->AddImportedMesh(ImportMesh);
		}
	}

	for (UniquePtr<UHMaterial>& ImportMaterial : ImportedData.ImportedMaterial)
	{
		std::filesystem::path OutPath = MaterialOutputPath + "/" + ImportMaterial->GetName() + GMaterialAssetExtension;
		if (!std::filesystem::exists(OutPath))
		{
			ImportMaterial->GenerateDefaultMaterialNodes();
			ImportMaterial->Export(OutPath);
			AssetMgr->AddImportedMaterial(GMaterialAssetPath + ImportMaterial->GetSourcePath() + GMaterialAssetExtension);
		}
	}

	// create scene object after import
	if (bCreateSceneObjectAfterImport)
	{
		// reset scene if it's creating a new scene, or it will add objects on the top of the current scene
		if (bCreateNewScene)
		{
			Engine->ResetScene();
		}

		UHDeferredShadingRenderer* Renderer = Engine->GetSceneRenderer();
		UHScene* Scene = Engine->GetScene();
		UHEditor* Editor = Engine->GetEditor();
		UHGraphic* Gfx = Engine->GetGfx();

		if (!bCreateNewScene)
		{
			// still need to wait previous render tasks before adding objects
			Renderer->WaitPreviousRenderTask();
			Gfx->WaitGPU();
			Renderer->Release();
		}

		// add mesh objects
		if (MeshCache.size() > 0)
		{
			std::vector<UHMeshRendererComponent*> MeshRenderers(MeshCache.size());

			for (size_t Idx = 0; Idx < MeshRenderers.size(); Idx++)
			{
				UHMesh* Mesh = AssetMgr->GetMesh(MeshCache[Idx]->GetSourcePath());
				if (Mesh == nullptr)
				{
					UHE_LOG("Mesh " + MeshCache[Idx]->GetSourcePath() + " is not found when adding renderer.\n");
					continue;
				}
				UHMaterial* Mat = AssetMgr->GetMaterial(Mesh->GetImportedMaterialName());

				MeshRenderers[Idx] = (UHMeshRendererComponent*)Scene->RequestComponent(UHMeshRendererComponent::ClassId);
				MeshRenderers[Idx]->SetMesh(Mesh);
				MeshRenderers[Idx]->SetMaterial(Mat);
				MeshRenderers[Idx]->SetPosition(Mesh->GetImportedTranslation());
				MeshRenderers[Idx]->SetRotation(Mesh->GetImportedRotation());
				MeshRenderers[Idx]->SetScale(Mesh->GetImportedScale());
			}
		}

		// add camera objects
		if (ImportedData.ImportedCameraData.size() > 0)
		{
			for (size_t Idx = 0; Idx < ImportedData.ImportedCameraData.size(); Idx++)
			{
				// request camera component and set properties
				UHCameraComponent* Camera = (UHCameraComponent*)Scene->RequestComponent(UHCameraComponent::ClassId);
				const UHFbxCameraData& CamData = ImportedData.ImportedCameraData[Idx];

				Camera->SetNearPlane(CamData.NearPlane);
				Camera->SetCullingDistance(CamData.CullingDistance);
				Camera->SetFov(CamData.Fov);
				Camera->SetPosition(CamData.Position);
				Camera->SetRotation(CamData.Rotation);
			}
		}

		// add light objects
		if (ImportedData.ImportedLightData.size() > 0)
		{
			for (size_t Idx = 0; Idx < ImportedData.ImportedLightData.size(); Idx++)
			{
				// request light component and set properties
				const UHFbxLightData& LightData = ImportedData.ImportedLightData[Idx];
				UHLightBase* NewLight = nullptr;

				if (LightData.Type == UHLightType::Directional)
				{
					UHDirectionalLightComponent* DirLight
						= (UHDirectionalLightComponent*)Scene->RequestComponent(UHDirectionalLightComponent::ClassId);

					NewLight = DirLight;
				}
				else if (LightData.Type == UHLightType::Point)
				{
					UHPointLightComponent* PointLight
						= (UHPointLightComponent*)Scene->RequestComponent(UHPointLightComponent::ClassId);

					PointLight->SetRadius(LightData.Radius);
					NewLight = PointLight;
				}
				else if (LightData.Type == UHLightType::Spot)
				{
					UHSpotLightComponent* SpotLight
						= (UHSpotLightComponent*)Scene->RequestComponent(UHSpotLightComponent::ClassId);

					SpotLight->SetRadius(LightData.Radius);
					SpotLight->SetAngle(LightData.SpotAngle);
					NewLight = SpotLight;
				}

				NewLight->SetLightColor(LightData.LightColor);
				NewLight->SetIntensity(LightData.Intensity);
				NewLight->SetPosition(LightData.Position);
				NewLight->SetRotation(LightData.Rotation);
			}
		}

		// add default camera and light when necessary
		if (Scene->GetMainCamera() == nullptr)
		{
			UHCameraComponent* DefaultCamera = (UHCameraComponent*)Scene->RequestComponent(UHCameraComponent::ClassId);
			DefaultCamera->SetPosition(XMFLOAT3(0, 2, -15));
			DefaultCamera->SetRotation(XMFLOAT3(0, -70, 0));
			DefaultCamera->SetCullingDistance(1000.0f);
		}

		if (Scene->GetDirLightCount() == 0
			&& Scene->GetPointLightCount() == 0
			&& Scene->GetSpotLightCount() == 0)
		{
			UHDirectionalLightComponent* DefaultLight = (UHDirectionalLightComponent*)Scene->RequestComponent(UHDirectionalLightComponent::ClassId);
			DefaultLight->SetLightColor(XMFLOAT3(0.95f, 0.91f, 0.6f));
			DefaultLight->SetIntensity(3.0f);
			DefaultLight->SetRotation(XMFLOAT3(45, -120, 0));
		}

		if (Scene->GetSkyLight() == nullptr)
		{
			UHSkyLightComponent* DefaultSkyLight = (UHSkyLightComponent*)Scene->RequestComponent(UHSkyLightComponent::ClassId);
			DefaultSkyLight->SetSkyColor(XMFLOAT3(0.8f, 0.8f, 0.8f));
			DefaultSkyLight->SetGroundColor(XMFLOAT3(0.3f, 0.3f, 0.3f));
			DefaultSkyLight->SetSkyIntensity(0.5f);
			DefaultSkyLight->SetGroundIntensity(0.3f);

			UHTextureCube* TexCube = AssetMgr->GetCubemapByName("UHDefaultSkyCube");

			VkCommandBuffer CreationCmd = Gfx->BeginOneTimeCmd();
			UHRenderBuilder RenderBuilder(Gfx, CreationCmd);
			TexCube->Build(Gfx, RenderBuilder);
			Gfx->EndOneTimeCmd(CreationCmd);

			DefaultSkyLight->SetCubemap(TexCube);
		}

		// post loading scene init
		Scene->Initialize(Engine);
		Renderer->Initialize(Scene);
		Renderer->InitRenderingResources();
		Editor->RefreshWorldDialog();
	}

	MessageBoxA(nullptr, "Import finished!", "Fbx Import", MB_OK);
}

#endif