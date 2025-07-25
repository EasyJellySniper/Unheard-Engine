#include "MeshDialog.h"

#if WITH_EDITOR
#include "Runtime/Engine/Asset.h"
#include "Runtime/Engine/Graphic.h"

UHMeshDialog::UHMeshDialog(UHAssetManager* InAsset, UHGraphic* InGfx)
	: UHDialog(nullptr, nullptr)
	, AssetMgr(InAsset)
	, CurrentMeshIndex(UHINDEXNONE)
	, CurrentTextureDS(nullptr)
{
	PreviewScene = MakeUnique<UHPreviewScene>(InGfx, UHPreviewSceneType::MeshPreview);

	// setup ImGui texture as well
	UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	LinearClampedInfo.MaxAnisotropy = 1;
	const UHSampler* LinearSampler = InGfx->RequestTextureSampler(LinearClampedInfo);

	CurrentTextureDS = ImGui_ImplVulkan_AddTexture(LinearSampler->GetSampler(), PreviewScene->GetPreviewRT()->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

UHMeshDialog::~UHMeshDialog()
{
	UH_SAFE_RELEASE(PreviewScene);
	CurrentTextureDS = nullptr;
}

void UHMeshDialog::ShowDialog()
{
	if (bIsOpened)
	{
		return;
	}

	UHDialog::ShowDialog();
	CurrentMeshIndex = UHINDEXNONE;
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
		ImGui::Text("Select the mesh to preview");

		if (ImGui::BeginListBox("##", ImVec2(-FLT_MIN, -FLT_MIN)))
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
			ImGui::Text(("Number of triangles: " + std::to_string(Meshes[CurrentMeshIndex]->GetIndicesCount() / 3)).c_str());
			ImGui::Image(CurrentTextureDS, ImVec2(512, 512));
		}

		ImGui::EndTable();
	}

	const bool bIsWindowActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	PreviewScene->Render(bIsWindowActive);
	ImGui::End();

	bIsDialogActive |= bIsWindowActive;
}

void UHMeshDialog::SelectMesh(UHMesh* InMesh)
{
	PreviewScene->SetMesh(InMesh);
}

#endif