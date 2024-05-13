#include "MeshDialog.h"

#if WITH_EDITOR
#include "Runtime/Renderer/DeferredShadingRenderer.h"
#include "Runtime/Engine/Asset.h"
#include "Runtime/Engine/Graphic.h"
#include "../Classes/EditorUtils.h"
#include "Runtime/Classes/AssetPath.h"
#include "../../Editor/Classes/FbxImporter.h"

UHMeshDialog::UHMeshDialog(UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer, UHRawInput* InInput)
	: UHDialog(nullptr, nullptr)
	, AssetMgr(InAsset)
	, Gfx(InGfx)
	, Renderer(InRenderer)
	, Input(InInput)
	, CurrentMeshIndex(UHINDEXNONE)
	, CurrentTextureDS(nullptr)
	, bCreateRendererAfterImport(false)
{
	PreviewScene = MakeUnique<UHPreviewScene>(Gfx, UHPreviewSceneType::MeshPreview);
	FBXImporterInterface = MakeUnique<UHFbxImporter>();

	// setup ImGui texture as well
	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	const UHSampler* LinearSampler = Gfx->RequestTextureSampler(LinearClampedInfo);

	CurrentTextureDS = ImGui_ImplVulkan_AddTexture(LinearSampler->GetSampler(), PreviewScene->GetPreviewRT()->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

UHMeshDialog::~UHMeshDialog()
{
	UH_SAFE_RELEASE(PreviewScene);
	CurrentTextureDS = nullptr;
	FBXImporterInterface.reset();
}

void UHMeshDialog::ShowDialog()
{
	if (bIsOpened)
	{
		return;
	}

	UHDialog::ShowDialog();
	CurrentMeshIndex = UHINDEXNONE;
	InputSourceFile = "";
	MeshOutputPath = "";
	MaterialOutputPath = "";
}

void UHMeshDialog::Update(bool& bIsDialogActive)
{
	if (!bIsOpened)
	{
		return;
	}

	const std::vector<UHMesh*>& Meshes = AssetMgr->GetUHMeshes();
	ImGui::Begin("Mesh Editor", &bIsOpened);

	if (ImGui::BeginTable("Meshes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
	{
		// left column
		ImGui::TableNextColumn();
		ImGui::Text("Select the mesh to edit");
		const ImVec2 WndSize = ImGui::GetWindowSize();

		if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, WndSize.y * 0.6f)))
		{
			for (int32_t Idx = 0; Idx < static_cast<int32_t>(Meshes.size()); Idx++)
			{
				bool bIsSelected = (CurrentMeshIndex == Idx);
				if (ImGui::Selectable(Meshes[Idx]->GetSourcePath().c_str(), &bIsSelected))
				{	
					CurrentMeshIndex = Idx;
					SelectMesh(Meshes[Idx]);
				}
			}
			ImGui::EndListBox();
		}
		ImGui::NewLine();

		// right column
		ImGui::TableNextColumn();
		if (CurrentMeshIndex != UHINDEXNONE)
		{
			ImGui::Text(("Number of triangles: " + std::to_string(Meshes[CurrentMeshIndex]->GetVertexCount() / 3)).c_str());
			ImGui::Image(CurrentTextureDS, ImVec2(512, 512));
		}

		ImGui::EndTable();
	}

	// fbx importing
	ImGui::BeginChild("Mesh import", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::Text("Mesh import>>>>>>>>>>");
	if (ImGui::BeginTable("Mesh Creation", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableNextColumn();
		ImGui::Checkbox("Create Renderer after import", &bCreateRendererAfterImport);
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
			InputSourceFile = UHUtilities::ToStringA(UHEditorUtil::FileSelectInput({ {L"FBX Formats"}, { L"*.fbx"} }));
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
	ImGui::EndChild();

	const bool bIsWindowActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	PreviewScene->Render(bIsWindowActive);
	ImGui::End();

	bIsDialogActive |= bIsWindowActive;
}

void UHMeshDialog::SelectMesh(UHMesh* InMesh)
{
	PreviewScene->SetMesh(InMesh);
}

bool IsPathValid(const std::filesystem::path& OutputFolder, std::filesystem::path AssetPath)
{
	AssetPath = std::filesystem::absolute(AssetPath);

	bool bIsValidOutputFolder = false;
	bIsValidOutputFolder |= UHUtilities::StringFind(OutputFolder.string() + "\\", AssetPath.string());
	bIsValidOutputFolder |= UHUtilities::StringFind(AssetPath.string(), OutputFolder.string() + "\\");
	return bIsValidOutputFolder && std::filesystem::exists(OutputFolder);
}

void UHMeshDialog::OnImport()
{
	const std::filesystem::path InputSource = InputSourceFile;
	if (!std::filesystem::exists(InputSource))
	{
		MessageBoxA(nullptr, "Invalid input source!", "Mesh Import", MB_OK);
		return;
	}

	if (!IsPathValid(MeshOutputPath, GMeshAssetFolder))
	{
		MessageBoxA(nullptr, "Invalid mesh output folder or it's not under the engine path Assets/Meshes!", "Mesh Import", MB_OK);
		return;
	}

	if (!IsPathValid(MaterialOutputPath, GMaterialAssetPath))
	{
		MessageBoxA(nullptr, "Invalid output folder or it's not under the engine path Assets/Materials!", "Mesh Import", MB_OK);
		return;
	}

	if (!IsPathValid(TextureReferencePath, GTextureAssetFolder))
	{
		MessageBoxA(nullptr, "Invalid texture reference folder or it's not under the engine path Assets/Textures!", "Mesh Import", MB_OK);
		return;
	}

	// import fbx
	std::vector<UniquePtr<UHMesh>> ImportedMeshes;
	std::vector<UniquePtr<UHMaterial>> ImportedMaterials;
	ImportedMeshes.reserve(100);
	ImportedMaterials.reserve(100);
	FBXImporterInterface->ImportRawFbx(InputSourceFile, TextureReferencePath, ImportedMeshes, ImportedMaterials);

	// Add imported meshes/materials to system, if the assets are already there, do not export them.
	// @TODO: consider a force overwriting option in the future
	const std::vector<UHMesh*>& Meshes = AssetMgr->GetUHMeshes();
	const std::vector<UHMaterial*>& Materials = AssetMgr->GetMaterials();

	for (UniquePtr<UHMesh>& ImportMesh : ImportedMeshes)
	{
		std::filesystem::path OutPath = MeshOutputPath + "/" + ImportMesh->GetName() + GMeshAssetExtension;
		ImportMesh->SetSourcePath(std::filesystem::relative(MeshOutputPath, GMeshAssetFolder).string() + "/" + ImportMesh->GetName());
		if (!std::filesystem::exists(OutPath))
		{
			ImportMesh->Export(OutPath);
			AssetMgr->AddImportedMesh(ImportMesh);
		}
	}

	for (UniquePtr<UHMaterial>& ImportMaterial : ImportedMaterials)
	{
		std::filesystem::path OutPath = MaterialOutputPath + "/" + ImportMaterial->GetName() + GMaterialAssetExtension;
		ImportMaterial->SetSourcePath(std::filesystem::relative(MaterialOutputPath, GMaterialAssetPath).string() + "/" + ImportMaterial->GetName());
		if (!std::filesystem::exists(OutPath))
		{
			ImportMaterial->GenerateDefaultMaterialNodes();
			ImportMaterial->Export(OutPath);
			AssetMgr->AddImportedMaterial(Gfx, GMaterialAssetPath + ImportMaterial->GetSourcePath() + GMaterialAssetExtension);
		}
	}

	// request mesh renderer from the scene if option is enabled
	if (bCreateRendererAfterImport)
	{
		UHScene* Scene = Renderer->GetCurrentScene();
		std::vector<UHMeshRendererComponent*> MeshRenderers(ImportedMeshes.size());

		for (size_t Idx = 0; Idx < MeshRenderers.size(); Idx++)
		{
			UHMesh* Mesh = AssetMgr->GetMesh(ImportedMeshes[Idx]->GetSourcePath());
			UHMaterial* Mat = AssetMgr->GetMaterial(Mesh->GetImportedMaterialName());

			MeshRenderers[Idx] = (UHMeshRendererComponent*)Scene->RequestComponent(UHMeshRendererComponent::ClassId);
			MeshRenderers[Idx]->SetMesh(Mesh);
			MeshRenderers[Idx]->SetMaterial(Mat);
			MeshRenderers[Idx]->SetPosition(Mesh->GetImportedTranslation());
			MeshRenderers[Idx]->SetRotation(Mesh->GetImportedRotation());
			MeshRenderers[Idx]->SetScale(Mesh->GetImportedScale());
		}

		// in the end, append mesh renderers to the scene renderer
		Renderer->AppendMeshRenderers(MeshRenderers);
	}

	ImportedMeshes.clear();
	ImportedMaterials.clear();
	MessageBoxA(nullptr, "Import finished!", "Mesh Import", MB_OK);
}

#endif