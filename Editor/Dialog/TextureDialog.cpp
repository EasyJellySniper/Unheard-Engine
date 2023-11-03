#include "TextureDialog.h"

#if WITH_EDITOR

#include "../Classes/EditorUtils.h"
#include "../../resource.h"
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Classes/Texture2D.h"
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
    TextureCreationDialog = MakeUnique<UHTextureCreationDialog>(Gfx, this, &TextureImporter);
    CurrentTextureIndex = UHINDEXNONE;
    CurrentTexture = nullptr;

    if (CurrentTextureDS)
    {
        Gfx->WaitGPU();
        ImGui_ImplVulkan_RemoveTexture(CurrentTextureDS);
    }
    CurrentTextureDS = nullptr;
}

void UHTextureDialog::Update()
{
    if (!bIsOpened)
    {
        return;
    }

    // check the creation before ImGui kicks off
    TextureCreationDialog->CheckPendingTextureCreation();

    // check if there is any texture DS to remove
    if (TextureDSToRemove.size() > 0)
    {
        Gfx->WaitGPU();
        for (VkDescriptorSet DS : TextureDSToRemove)
        {
            ImGui_ImplVulkan_RemoveTexture(DS);
        }
        TextureDSToRemove.clear();
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
            const float ImgSize = ImGui::GetColumnWidth() * 0.9f;
            const float AspectRatioHW = (float)CurrentTexture->GetExtent().height / (float)CurrentTexture->GetExtent().width;
            ImGui::Image(CurrentTextureDS, ImVec2(ImgSize, ImgSize * AspectRatioHW));
        }

        ImGui::EndTable();
    }

    ImGui::NewLine();
    TextureCreationDialog->Update();
    ImGui::End();
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
        MessageBoxA(nullptr, "Please select a valid image source file and try again.", "Missing source file", MB_OK);
        return;
    }
    CurrentTexture->SetRawSourcePath(RawSourcePath.string());

    // recreate texture if any option is changed
    const UHTextureSettings OldSetting = CurrentTexture->GetTextureSettings();
    UHTextureSettings NewSetting = CurrentEditingSettings;
    NewSetting.bIsHDR = (RawSourcePath.extension().string() == ".exr");
    ValidateTextureSetting(NewSetting);
    CurrentTexture->SetTextureSettings(NewSetting);

    UHStatusDialogScope StatusDialog("Processing...");

    // always recreating texture
    uint32_t W, H;
    std::vector<uint8_t> RawData = TextureImporter.LoadTexture(RawSourcePath, W, H);
    CurrentTexture->SetExtent(W, H);
    CurrentTexture->SetTextureData(RawData);
    CurrentTexture->Recreate(true);
    Renderer->UpdateTextureDescriptors();

    const bool bIsNormalChanged = NewSetting.bIsNormal != OldSetting.bIsNormal || NewSetting.CompressionSetting != OldSetting.CompressionSetting;
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
    MessageBoxA(nullptr, "Current editing texture is saved.\nIt's also recommended to resave referencing materials.", "Texture Editor", MB_OK);
}

void UHTextureDialog::ControlSaveAll()
{
    // iterate all textures and save them
    for (UHTexture2D* Tex : AssetMgr->GetTexture2Ds())
    {
        const std::filesystem::path SourcePath = Tex->GetSourcePath();
        Tex->Export(GTextureAssetFolder + SourcePath.string());
    }
    MessageBoxA(nullptr, "All textures are saved.", "Texture Editor", MB_OK);
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
    UHSamplerInfo LinearClampedInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    LinearClampedInfo.MaxAnisotropy = 1;
    LinearClampedInfo.MipBias = static_cast<float>(CurrentMip);

    // though this might generate more sampler variations because of mip bias setting
    // it's fine to do so in editor
    const UHSampler* LinearSampler = Gfx->RequestTextureSampler(LinearClampedInfo);
    if (CurrentTextureDS != nullptr)
    {
        TextureDSToRemove.push_back(CurrentTextureDS);
    }
    CurrentTextureDS = ImGui_ImplVulkan_AddTexture(LinearSampler->GetSampler(), CurrentTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

#endif