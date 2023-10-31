#include "TextureCreationDialog.h"

#if WITH_EDITOR
#include "../../resource.h"
#include "../Classes/EditorUtils.h"
#include "../../Runtime/Classes/Utility.h"
#include <filesystem>
#include "TextureDialog.h"
#include "CubemapDialog.h"
#include "../../Runtime/Classes/AssetPath.h"
#include "../../Runtime/Engine/Asset.h"
#include "../../Runtime/Engine/Graphic.h"
#include "../../Runtime/Renderer/RenderBuilder.h"
#include "StatusDialog.h"

UHTextureCreationDialog::UHTextureCreationDialog(UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter)
	: UHDialog(nullptr, nullptr)
    , CurrentEditingSettings(UHTextureSettings())
    , TextureImporter(InTextureImporter)
    , TextureDialog(InTextureDialog)
    , CubemapDialog(nullptr)
    , AssetMgr(nullptr)
    , Gfx(InGfx)
{
    CurrentOutputPath = "Assets\\Textures";
}

UHTextureCreationDialog::UHTextureCreationDialog(UHGraphic* InGfx, UHCubemapDialog* InCubemapDialog, UHAssetManager* InAssetMgr)
    : UHDialog(nullptr, nullptr)
    , CurrentEditingSettings(UHTextureSettings())
    , CubemapDialog(InCubemapDialog)
    , TextureDialog(nullptr)
    , TextureImporter(nullptr)
    , AssetMgr(InAssetMgr)
    , Gfx(InGfx)
{
    CurrentOutputPath = "Assets\\Textures";
}

void UHTextureCreationDialog::Update()
{
    if (TextureDialog)
    {
        ShowTexture2DCreationUI();
    }
    else if (CubemapDialog)
    {
        ShowCubemapCreationUI();
    }
}

void UHTextureCreationDialog::ShowTexture2DCreationUI()
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

void UHTextureCreationDialog::ShowCubemapCreationUI()
{
    ImGui::BeginChild("Cubemap Creation", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Cubemap Creation, use either 6 slices or 1 panorama UHTexture2D for creation.");

    // output path
    ImGui::Text("Output Path:");
    ImGui::Text(CurrentOutputPath.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse Output"))
    {
        ControlBrowserOutputFolder();
    }

    if (ImGui::BeginTable("Cubemap Creation", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableNextColumn();

        const std::vector<UHTexture2D*> Textures = AssetMgr->GetTexture2Ds();
        const std::string SliceName[6] = { "+X","-X","+Y","-Y","+Z","-Z" };

        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            if (ImGui::BeginCombo(SliceName[Idx].c_str(), CurrentSelectedSlice[Idx].c_str()))
            {
                for (size_t Tdx = 0; Tdx < Textures.size(); Tdx++)
                {
                    const bool bIsSelected = (Textures[Tdx]->GetSourcePath() == CurrentSelectedSlice[Tdx]);
                    if (ImGui::Selectable(Textures[Tdx]->GetSourcePath().c_str(), bIsSelected))
                    {
                        CurrentSelectedSlice[Idx] = Textures[Tdx]->GetSourcePath();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Create from 6 slices"))
        {
            ControlCubemapCreate(false);
        }

        ImGui::TableNextColumn();

        if (ImGui::BeginCombo("Panorama", CurrentSelectedPanorama.c_str()))
        {
            for (size_t Tdx = 0; Tdx < Textures.size(); Tdx++)
            {
                const bool bIsSelected = (Textures[Tdx]->GetSourcePath() == CurrentSelectedPanorama);
                if (ImGui::Selectable(Textures[Tdx]->GetSourcePath().c_str(), bIsSelected))
                {
                    CurrentSelectedPanorama = Textures[Tdx]->GetSourcePath();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Create from panorama"))
        {
            ControlCubemapCreate(true);
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

    bool bIsValidOutputFolder = false;
    bIsValidOutputFolder |= UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());
    bIsValidOutputFolder |= UHUtilities::StringFind(TextureAssetPath.string(), OutputFolder.string() + "\\");

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
    if (NewTex)
    {
        TextureDialog->OnCreationFinished(NewTex);
    }
    else
    {
        MessageBoxA(nullptr, "Texture creation failed.", "Texture Creation", MB_OK);
    }
}

void UHTextureCreationDialog::ControlCubemapCreate(bool bIsPanorama)
{
    std::filesystem::path OutputFolder = CurrentOutputPath;
    std::filesystem::path TextureAssetPath = GTextureAssetFolder;
    TextureAssetPath = std::filesystem::absolute(TextureAssetPath);

    bool bIsValidOutputFolder = false;
    bIsValidOutputFolder |= UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());
    bIsValidOutputFolder |= UHUtilities::StringFind(TextureAssetPath.string(), OutputFolder.string() + "\\");

    if (!std::filesystem::exists(OutputFolder) || !bIsValidOutputFolder)
    {
        MessageBoxA(nullptr, "Invalid output folder or it's not under the engine path Assets/Textures!", "Cubemap Creation", MB_OK);
        return;
    }

    // remove the project folder
    OutputFolder = std::filesystem::relative(OutputFolder);

    if (bIsPanorama)
    {
        // @TODO: Implement Panorama to Cubemap conversion
    }
    else
    {
        std::vector<UHTexture2D*> Slices(6);
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            Slices[Idx] = AssetMgr->GetTexture2DByPath(CurrentSelectedSlice[Idx]);
        }

        bool bValidSlices = true;
        UHTexture2D* BaseSlice = Slices[0];
        for (int32_t Idx = 0; Idx < 6 && bValidSlices; Idx++)
        {
            bValidSlices &= (Slices[Idx] != nullptr);
            bValidSlices &= (BaseSlice->GetFormat() == Slices[Idx]->GetFormat());
            bValidSlices &= (BaseSlice->GetExtent().width == Slices[Idx]->GetExtent().width
                && BaseSlice->GetExtent().height == Slices[Idx]->GetExtent().height);
        }

        if (!bValidSlices)
        {
            MessageBoxA(nullptr, "Invalid slices! Make sure that all slices have the same format & dimension.", "Cubemap Creation", MB_OK);
            return;
        }

        // @TODO: Provide rename funtion somewhere else
        UHTextureCube* NewCube = Gfx->RequestTextureCube(Slices[0]->GetName() + "_Cube", Slices);
        if (NewCube)
        {
            std::string OutputPathName = OutputFolder.string() + "/" + NewCube->GetName();
            std::string SavedPathName = UHUtilities::StringReplace(OutputPathName, "\\", "/");
            SavedPathName = UHUtilities::StringReplace(SavedPathName, GTextureAssetFolder, "");
            NewCube->SetSourcePath(SavedPathName);

            // build the cubemap
            {
                VkCommandBuffer Cmd = Gfx->BeginOneTimeCmd();
                UHRenderBuilder Builder(Gfx, Cmd);
                NewCube->Build(Gfx, Cmd, Builder);
                Gfx->EndOneTimeCmd(Cmd);
            }
            NewCube->Export(GTextureAssetFolder + "/" + NewCube->GetSourcePath());

            CubemapDialog->OnCreationFinished(NewCube);
        }
        else
        {
            MessageBoxA(nullptr, "Cubemap creation failed.", "Cubemap Creation", MB_OK);
        }
    }
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