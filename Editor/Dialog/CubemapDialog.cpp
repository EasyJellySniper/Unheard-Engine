#include "CubemapDialog.h"

// cubemap editor impl
// cubemap generation simply takes imported panorama UHTextureCube for now
#if WITH_EDITOR
#include "../Classes/EditorUtils.h"
#include "../../resource.h"
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Classes/TextureCube.h"
#include "../../Runtime/Renderer/RenderBuilder.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Classes/AssetPath.h"
#include <filesystem>
#include "StatusDialog.h"

// texture creation
UHCubemapDialog::UHCubemapDialog(UHAssetManager* InAssetMgr, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer)
    : UHDialog(nullptr, nullptr)
    , AssetMgr(InAssetMgr)
    , Gfx(InGfx)
    , Renderer(InRenderer)
    , CurrentCube(nullptr)
    , CurrentCubeIndex(-1)
    , CurrentMip(0)
    , CurrentEditingSettings(UHTextureSettings())
{
    for (int32_t Idx = 0; Idx < 6; Idx++)
    {
        CurrentCubeDS[Idx] = nullptr;
    }
}

void UHCubemapDialog::ShowDialog()
{
    if (bIsOpened)
    {
        return;
    }

    UHDialog::ShowDialog();
    Init();
}

void UHCubemapDialog::Init()
{
    TextureCreationDialog = MakeUnique<UHTextureCreationDialog>(Gfx, this, AssetMgr);
    CurrentCubeIndex = UHINDEXNONE;
    CurrentCube = nullptr;
    for (int32_t Idx = 0; Idx < 6; Idx++)
    {
        CurrentCubeDS[Idx] = nullptr;
    }
}

void UHCubemapDialog::Update()
{
    if (!bIsOpened)
    {
        return;
    }

    // check the creation before ImGui kicks off
    TextureCreationDialog->CheckPendingCubeCreation();

    // check if there is any texture DS to remove
    if (CubeDSToRemove.size() > 0)
    {
        Gfx->WaitGPU();
        for (VkDescriptorSet DS : CubeDSToRemove)
        {
            ImGui_ImplVulkan_RemoveTexture(DS);
        }
        CubeDSToRemove.clear();
    }

    ImGui::Begin("Cubemap Editor", &bIsOpened, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("Cubemaps", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableNextColumn();
        ImGui::Text("Select the cubemap to edit");

        const ImVec2 WndSize = ImGui::GetWindowSize();

        if (ImGui::BeginListBox("##", ImVec2(0, WndSize.y * 0.6f)))
        {
            const std::vector<UHTextureCube*>& Cubes = AssetMgr->GetCubemaps();
            for (int32_t Idx = 0; Idx < static_cast<int32_t>(Cubes.size()); Idx++)
            {
                bool bIsSelected = (CurrentCubeIndex == Idx);
                if (ImGui::Selectable(Cubes[Idx]->GetSourcePath().c_str(), &bIsSelected))
                {
                    SelectCubemap(Idx);
                    CurrentCubeIndex = Idx;
                }
            }
            ImGui::EndListBox();
        }
        ImGui::NewLine();

        std::string CompressionModeTextA = UHUtilities::ToStringA(GCompressionModeText[CurrentEditingSettings.CompressionSetting]);
        ImGui::Text(("Compression Mode:" + CompressionModeTextA).c_str());

        ImGui::Text("Source File:");
        ImGui::SameLine();
        if (CurrentCube)
        {
            ImGui::Text((CurrentCube->GetSourcePath() + GCubemapAssetExtension).c_str());
        }

        if (ImGui::Button("Save"))
        {
            ControlSave();
        }

        ImGui::SameLine();
        if (ImGui::Button("Save All"))
        {
            ControlSaveAll();
        }

        ImGui::TableNextColumn();

        // size text
        if (CurrentCube)
        {
            const VkExtent2D TexExtent = CurrentCube->GetExtent();
            std::string SizeText = "Dimension: " + std::to_string(TexExtent.width) + " x " + std::to_string(TexExtent.height);

            SizeText += " (" + std::to_string(CurrentCube->GetDataSize() / 1024) + " KB)";
            ImGui::Text(SizeText.c_str());
        }

        // mip slider
        if (CurrentCube)
        {
            ImGui::SameLine();
            int32_t MipCount = CurrentCube->GetMipMapCount();
            if (ImGui::SliderInt("MipLevel", &CurrentMip, 0, MipCount - 1))
            {
                RefreshImGuiMipLevel();
            }
        }

        // cube preview
        bool bCubeValid = true;
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            bCubeValid &= (CurrentCubeDS[Idx] != nullptr);
        }

        if (bCubeValid)
        {
            const float ImgSize = (ImGui::GetColumnWidth() * 0.95f) / 4.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGui::InvisibleButton("##padding", ImVec2(ImgSize, 1));
            ImGui::SameLine();
            ImGui::Image(CurrentCubeDS[2], ImVec2(ImgSize, ImgSize));

            ImGui::Image(CurrentCubeDS[1], ImVec2(ImgSize, ImgSize));
            ImGui::SameLine();
            ImGui::Image(CurrentCubeDS[4], ImVec2(ImgSize, ImgSize));
            ImGui::SameLine();
            ImGui::Image(CurrentCubeDS[0], ImVec2(ImgSize, ImgSize));
            ImGui::SameLine();
            ImGui::Image(CurrentCubeDS[5], ImVec2(ImgSize, ImgSize));

            ImGui::InvisibleButton("##padding", ImVec2(ImgSize, 1));
            ImGui::SameLine();
            ImGui::Image(CurrentCubeDS[3], ImVec2(ImgSize, ImgSize));

            ImGui::PopStyleVar();
        }

        ImGui::EndTable();
    }

    ImGui::NewLine();
    TextureCreationDialog->Update();
    ImGui::End();
}

void UHCubemapDialog::OnCreationFinished(UHTexture* InTexture)
{
    if (!UHUtilities::FindByElement(AssetMgr->GetCubemaps(), dynamic_cast<UHTextureCube*>(InTexture)))
    {
        AssetMgr->AddCubemap(dynamic_cast<UHTextureCube*>(InTexture));
    }

    // force selecting the created texture
    const int32_t NewIdx = UHUtilities::FindIndex(AssetMgr->GetCubemaps(), static_cast<UHTextureCube*>(InTexture));
    SelectCubemap(NewIdx);
    CurrentCubeIndex = NewIdx;
    Renderer->UpdateTextureDescriptors();
    Renderer->RefreshSkyLight(false);
}

void UHCubemapDialog::SelectCubemap(int32_t TexIndex)
{
    CurrentCube = AssetMgr->GetCubemaps()[TexIndex];
    if (!CurrentCube->IsBuilt())
    {
        // upload to gpu if it's not in resident
        VkCommandBuffer UploadCmd = Gfx->BeginOneTimeCmd();
        UHRenderBuilder UploadBuilder(Gfx, UploadCmd);
        CurrentCube->Build(Gfx, UploadBuilder);
        Gfx->EndOneTimeCmd(UploadCmd);
    }

    CurrentMip = 0;
    CurrentEditingSettings = CurrentCube->GetTextureSettings();

    // refresh ImGui texture
    RefreshImGuiMipLevel();
}

void UHCubemapDialog::ControlSave()
{
    if (CurrentCube == nullptr)
    {
        return;
    }

    const std::filesystem::path SourcePath = CurrentCube->GetSourcePath();
    CurrentCube->Export(GTextureAssetFolder + SourcePath.string());
    MessageBoxA(nullptr, "Current editing cube is saved.", "Cubemap Editor", MB_OK);
}

void UHCubemapDialog::ControlSaveAll()
{
    // iterate all textures and save them
    for (UHTextureCube* Tex : AssetMgr->GetCubemaps())
    {
        const std::filesystem::path SourcePath = Tex->GetSourcePath();
        Tex->Export(GTextureAssetFolder + SourcePath.string());
    }
    MessageBoxA(nullptr, "All cubes are saved.", "Cubemap Editor", MB_OK);
}

void UHCubemapDialog::RefreshImGuiMipLevel()
{
    UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    LinearClampedInfo.MaxAnisotropy = 1;
    LinearClampedInfo.MipBias = static_cast<float>(CurrentMip);

    const UHSampler* LinearSampler = Gfx->RequestTextureSampler(LinearClampedInfo);
    for (int32_t Idx = 0; Idx < 6; Idx++)
    {
        if (CurrentCubeDS[Idx] != nullptr)
        {
            CubeDSToRemove.push_back(CurrentCubeDS[Idx]);
        }
        CurrentCubeDS[Idx] = ImGui_ImplVulkan_AddTexture(LinearSampler->GetSampler(), CurrentCube->GetCubemapImageView(Idx), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

#endif