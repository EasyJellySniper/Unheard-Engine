#include "TextureDialog.h"

#if WITH_EDITOR

#include "../Classes/EditorUtils.h"
#include "../../resource.h"
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Classes/Texture2D.h"
#include "../Editor/PreviewScene.h"
#include "../../Runtime/Renderer/RenderBuilder.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Classes/AssetPath.h"
#include <filesystem>
#include "StatusDialog.h"

// texture creation
UHTextureDialog::UHTextureDialog(UHAssetManager* InAssetMgr, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer)
	: UHDialog(nullptr, nullptr)
	, AssetMgr(InAssetMgr)
    , Gfx(InGfx)
    , Renderer(InRenderer)
    , CurrentTexture(nullptr)
    , CurrentTextureIndex(-1)
    , CurrentMip(0)
    , CurrentEditingSettings(UHTextureSettings())
    , CurrentTextureDS(nullptr)
{
    TextureImporter = UHTextureImporter(InGfx);
}

void UHTextureDialog::ShowDialog()
{
    if (bIsOpened)
    {
        return;
    }

    UHDialog::ShowDialog();
    Init();
}

void UHTextureDialog::Init()
{
    TextureCreationDialog = MakeUnique<UHTextureCreationDialog>(ParentWindow, Gfx, this, &TextureImporter);
    CurrentTextureIndex = UHINDEXNONE;
    CurrentTexture = nullptr;
}

void UHTextureDialog::Update()
{
    if (!bIsOpened)
    {
        return;
    }

    ImGui::Begin("Texture Editor", &bIsOpened, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTable("Texture2Ds", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableNextColumn();
        ImGui::Text("Select the texture to edit");
        ImGui::Text("Setting with star(*) mark triggers a shader recompilation");

        const ImVec2 WndSize = ImGui::GetWindowSize();

        if (ImGui::BeginListBox("##", ImVec2(0, WndSize.y * 0.6f)))
        {
            const std::vector<UHTexture2D*>& Textures = AssetMgr->GetTexture2Ds();
            for (int32_t Idx = 0; Idx < static_cast<int32_t>(Textures.size()); Idx++)
            {
                bool bIsSelected = (CurrentTextureIndex == Idx);
                if (ImGui::Selectable(Textures[Idx]->GetSourcePath().c_str(), &bIsSelected))
                {
                    SelectTexture(Idx);
                    CurrentTextureIndex = Idx;
                }
            }
            ImGui::EndListBox();
        }
        ImGui::NewLine();

        std::string CompressionModeTextA = UHUtilities::ToStringA(GCompressionModeText[CurrentEditingSettings.CompressionSetting]);
        if (ImGui::BeginCombo("Compression Mode", CompressionModeTextA.c_str()))
        {
            for (size_t Idx = 0; Idx < GCompressionModeText.size(); Idx++)
            {
                const bool bIsSelected = (CurrentEditingSettings.CompressionSetting == Idx);
                std::string CompressionModeTextA = UHUtilities::ToStringA(GCompressionModeText[Idx]);
                if (ImGui::Selectable(CompressionModeTextA.c_str(), bIsSelected))
                {
                    CurrentEditingSettings.CompressionSetting = static_cast<UHTextureCompressionSettings>(Idx);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("Is Linear", &CurrentEditingSettings.bIsLinear);
        ImGui::Checkbox("Is Normal*", &CurrentEditingSettings.bIsNormal);
        ImGui::NewLine();
        if (ImGui::Button("Apply"))
        {
            ControlApply();
        }

        ImGui::Text("Source File:");
        ImGui::SameLine();
        if (CurrentTexture)
        {
            ImGui::Text(CurrentTexture->GetRawSourcePath().c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            ControlBrowseRawTexture();
        }

        if (ImGui::Button("Create"))
        {
            ControlTextureCreate();
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
        if (CurrentTexture)
        {
            const VkExtent2D TexExtent = CurrentTexture->GetExtent();
            std::string SizeText = "Dimension: " + std::to_string(TexExtent.width) + " x " + std::to_string(TexExtent.height);
            SizeText += " (" + std::to_string(CurrentTexture->GetTextureData().size() / 1024) + " KB)";
            ImGui::Text(SizeText.c_str());
        }

        // mip slider
        if (CurrentTexture)
        {
            ImGui::SameLine();
            int32_t MipCount = CurrentTexture->GetMipMapCount();
            if (ImGui::SliderInt("MipLevel", &CurrentMip, 0, MipCount - 1))
            {
                RefreshImGuiMipLevel();
            }
        }

        // texture preview
        if (CurrentTextureDS)
        {
            const float ImgSize = WndSize.x * 0.45f;
            const float AspectRatioHW = (float)CurrentTexture->GetExtent().height / (float)CurrentTexture->GetExtent().width;
            ImGui::Image(CurrentTextureDS, ImVec2(ImgSize, ImgSize * AspectRatioHW));
        }

        ImGui::EndTable();
    }
    ImGui::End();

    TextureCreationDialog->Update();
}

void UHTextureDialog::OnCreationFinished(UHTexture* InTexture)
{
    if (!UHUtilities::FindByElement(AssetMgr->GetTexture2Ds(), dynamic_cast<UHTexture2D*>(InTexture)))
    {
        AssetMgr->AddTexture2D(dynamic_cast<UHTexture2D*>(InTexture));
    }

    // force selecting the created texture
    const int32_t NewIdx = UHUtilities::FindIndex(AssetMgr->GetTexture2Ds(), static_cast<UHTexture2D*>(InTexture));
    SelectTexture(NewIdx);
    CurrentTextureIndex = NewIdx;
    Renderer->UpdateTextureDescriptors();
}

void UHTextureDialog::SelectTexture(int32_t TexIndex)
{
    CurrentTexture = AssetMgr->GetTexture2Ds()[TexIndex];
    if (!CurrentTexture->HasUploadedToGPU())
    {
        // upload to gpu if it's not in resident
        VkCommandBuffer UploadCmd = Gfx->BeginOneTimeCmd();
        UHRenderBuilder UploadBuilder(Gfx, UploadCmd);
        CurrentTexture->UploadToGPU(Gfx, UploadCmd, UploadBuilder);
        Gfx->EndOneTimeCmd(UploadCmd);
    }

    CurrentMip = 0;
    CurrentEditingSettings = CurrentTexture->GetTextureSettings();

    // refresh ImGui texture
    RefreshImGuiMipLevel();
}

void UHTextureDialog::ControlApply()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    std::string RawSource = CurrentTexture->GetRawSourcePath();
    std::filesystem::path RawSourcePath(RawSource);
    if (!std::filesystem::exists(RawSourcePath))
    {
        MessageBoxA(ParentWindow, "Please select a valid image source file and try again.", "Missing source file", MB_OK);
        return;
    }
    CurrentTexture->SetRawSourcePath(RawSourcePath.string());

    // recreate texture if any option is changed
    const bool bIsLinear = CurrentEditingSettings.bIsLinear;
    const bool bIsNormal = CurrentEditingSettings.bIsNormal;
    const UHTextureSettings OldSetting = CurrentTexture->GetTextureSettings();

    UHTextureSettings NewSetting;
    NewSetting.bIsLinear = bIsLinear;
    NewSetting.bIsNormal = bIsNormal;
    NewSetting.CompressionSetting = CurrentEditingSettings.CompressionSetting;
    CurrentTexture->SetTextureSettings(NewSetting);

    if ((NewSetting.CompressionSetting == BC4 || NewSetting.CompressionSetting == BC5) && !bIsLinear)
    {
        MessageBoxA(ParentWindow, "BC4/BC5 can only be used with linear texture", "Error", MB_OK);
        return;
    }

    if (bIsNormal && NewSetting.CompressionSetting == BC4)
    {
        MessageBoxA(ParentWindow, "Normal texture needs at least 2 channels, please choose other compression mode", "Error", MB_OK);
        return;
    }

    UHStatusDialogScope StatusDialog("Processing...");

    // always recreating texture
    uint32_t W, H;
    std::vector<uint8_t> RawData = TextureImporter.LoadTexture(RawSourcePath.wstring(), W, H);
    CurrentTexture->SetExtent(W, H);
    CurrentTexture->SetTextureData(RawData);
    CurrentTexture->Recreate();
    Renderer->UpdateTextureDescriptors();

    const bool bIsNormalChanged = bIsNormal != OldSetting.bIsNormal || NewSetting.CompressionSetting != OldSetting.CompressionSetting;
    if (bIsNormalChanged)
    {
        // if normal is changed, all materials are referencing this texture needs a recompile
        for (UHObject* MatObj : CurrentTexture->GetReferenceObjects())
        {
            if (UHMaterial* Mat = static_cast<UHMaterial*>(MatObj))
            {
                Mat->SetCompileFlag(UHMaterialCompileFlag::FullCompileTemporary);
                Renderer->RefreshMaterialShaders(Mat);
            }
        }
    }

    SelectTexture(CurrentTextureIndex);
}

void UHTextureDialog::ControlSave()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    const std::filesystem::path SourcePath = CurrentTexture->GetSourcePath();
    CurrentTexture->Export(GTextureAssetFolder + SourcePath.string());
    MessageBoxA(ParentWindow, "Current editing texture is saved.\nIt's also recommended to resave referencing materials.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlSaveAll()
{
    // iterate all textures and save them
    for (UHTexture2D* Tex : AssetMgr->GetTexture2Ds())
    {
        const std::filesystem::path SourcePath = Tex->GetSourcePath();
        Tex->Export(GTextureAssetFolder + SourcePath.string());
    }
    MessageBoxA(ParentWindow, "All textures are saved.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlTextureCreate()
{
    TextureCreationDialog->ShowDialog();
}

void UHTextureDialog::ControlBrowseRawTexture()
{
    if (CurrentTexture == nullptr)
    {
        return;
    }

    std::filesystem::path FilePath = UHEditorUtil::FileSelectInput(GImageFilter);
    std::filesystem::path RawSourcePath = std::filesystem::relative(FilePath);
    if (RawSourcePath.string().empty())
    {
        // if the path is not relative to engine folder, just use the absolute path
        RawSourcePath = FilePath;
    }
    CurrentTexture->SetRawSourcePath(RawSourcePath.string());
}

void UHTextureDialog::RefreshImGuiMipLevel()
{
    UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    PointClampedInfo.MaxAnisotropy = 1;
    PointClampedInfo.MipBias = static_cast<float>(CurrentMip);

    // though this might generate more sampler variations because of mip bias setting
    // it's fine to do so in editor
    const UHSampler* PointSampler = Gfx->RequestTextureSampler(PointClampedInfo);
    if (CurrentTextureDS != nullptr)
    {
        Gfx->WaitGPU();
        ImGui_ImplVulkan_RemoveTexture(CurrentTextureDS);
    }
    CurrentTextureDS = ImGui_ImplVulkan_AddTexture(PointSampler->GetSampler(), CurrentTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

#endif