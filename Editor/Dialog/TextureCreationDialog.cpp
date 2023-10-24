#include "TextureCreationDialog.h"

#if WITH_EDITOR
#include "../../resource.h"
#include "../Classes/EditorUtils.h"
#include "../../Runtime/Classes/Utility.h"
#include <filesystem>
#include "TextureDialog.h"
#include "../../Runtime/Classes/AssetPath.h"
#include "StatusDialog.h"

UHTextureCreationDialog::UHTextureCreationDialog(UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter)
	: UHDialog(nullptr, nullptr)
    , CurrentEditingSettings(UHTextureSettings())
{
    TextureImporter = InTextureImporter;
    TextureDialog = InTextureDialog;
}

void UHTextureCreationDialog::Update()
{
    ImGui::BeginChild("Texture Creation", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Texture Creation>>>>>>>>>>");
    if (ImGui::BeginTable("Texture Creation", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableNextColumn();

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
        ImGui::Checkbox("Is Normal", &CurrentEditingSettings.bIsNormal);
        if (ImGui::Button("Create"))
        {
            ControlTextureCreate();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Input File:");
        ImGui::Text(CurrentSourceFile.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse"))
        {
            ControlBrowserInput();
        }

        ImGui::Text("Output Path:");
        ImGui::Text(CurrentOutputPath.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse Output"))
        {
            ControlBrowserOutputFolder();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void UHTextureCreationDialog::ControlTextureCreate()
{
    const std::filesystem::path InputSource = CurrentSourceFile;
    std::filesystem::path OutputFolder = CurrentOutputPath;
    std::filesystem::path TextureAssetPath = GTextureAssetFolder;
    TextureAssetPath = std::filesystem::absolute(TextureAssetPath);
    const bool bIsValidOutputFolder = UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());

    if (!std::filesystem::exists(InputSource))
    {
        MessageBoxA(nullptr, "Invalid input source!", "Texture Creation", MB_OK);
        return;
    }

    if (!std::filesystem::exists(OutputFolder) || !bIsValidOutputFolder)
    {
        MessageBoxA(nullptr, "Invalid output folder or it's not under the engine path Assets/Textures!", "Texture Creation", MB_OK);
        return;
    }

    // remove the project folder
    OutputFolder = std::filesystem::relative(OutputFolder);

    UHStatusDialogScope StatusDialog("Creating...");

    std::filesystem::path RawSourcePath = std::filesystem::relative(InputSource);
    if (RawSourcePath.string().empty())
    {
        RawSourcePath = InputSource;
    }

    UHTextureSettings TextureSetting = CurrentEditingSettings;
    TextureSetting.bIsHDR = (RawSourcePath.extension().string() == ".exr");
    ValidateTextureSetting(TextureSetting);

    UHTexture* NewTex = TextureImporter->ImportRawTexture(RawSourcePath, OutputFolder, TextureSetting);
    TextureDialog->OnCreationFinished(NewTex);
}

void UHTextureCreationDialog::ControlBrowserInput()
{
    CurrentSourceFile = UHUtilities::ToStringA(UHEditorUtil::FileSelectInput(GImageFilter));
}

void UHTextureCreationDialog::ControlBrowserOutputFolder()
{
    CurrentOutputPath = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder());
}

#endif