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
#include "../../Runtime/Renderer/RendererShared.h"
#include "StatusDialog.h"

UHTextureCreationDialog::UHTextureCreationDialog(UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter)
	: UHDialog(nullptr, nullptr)
    , CurrentEditingSettings(UHTextureSettings())
    , TextureImporter(InTextureImporter)
    , TextureDialog(InTextureDialog)
    , CubemapDialog(nullptr)
    , AssetMgr(nullptr)
    , Gfx(InGfx)
    , bNeedCreatingTexture(false)
    , bNeedCreatingCube(false)
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
    , bNeedCreatingTexture(false)
    , bNeedCreatingCube(false)
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

void UHTextureCreationDialog::CheckPendingTextureCreation()
{
    if (bNeedCreatingTexture)
    {
        ControlTextureCreate();
        bNeedCreatingTexture = false;
    }
}

void UHTextureCreationDialog::CheckPendingCubeCreation()
{
    if (bNeedCreatingCube)
    {
        ControlCubemapCreate();
        bNeedCreatingCube = false;
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
            // set creating flag as true and do the creation next frame
            bNeedCreatingTexture = true;
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
            bNeedCreatingCube = true;
            bCreatingCubeFromPanorama = false;
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
            bNeedCreatingCube = true;
            bCreatingCubeFromPanorama = true;
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

void UHTextureCreationDialog::ControlCubemapCreate()
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

    if (bCreatingCubeFromPanorama)
    {
        // Panorama to Cubemap conversion, use a shader pass for it
        UHTexture2D* InputTexture = AssetMgr->GetTexture2DByPath(CurrentSelectedPanorama);
        if (InputTexture == nullptr)
        {
            MessageBoxA(nullptr, "Invalid panorama texture input!", "Cubemap Creation", MB_OK);
            return;
        }

        UHStatusDialogScope StatusDialog("Creating...");

        // setup output dimension, follow the height of panorama map
        VkExtent2D OutputExtent;
        uint32_t Size = (std::min)(InputTexture->GetExtent().width, InputTexture->GetExtent().height);
        OutputExtent.width = Size;
        OutputExtent.height = Size;

        UHTextureSettings Settings = InputTexture->GetTextureSettings();
        UHTextureFormat UncompressedFormat = UH_FORMAT_NONE;
        if (Settings.bIsHDR)
        {
            UncompressedFormat = UH_FORMAT_RGBA16F;
        }
        else
        {
            UncompressedFormat = Settings.bIsLinear ? UH_FORMAT_RGBA8_UNORM : UH_FORMAT_RGBA8_SRGB;
        }

        // Step 1 ------------------------------------------------- Convert panorama to individual cube slice RT
        UHRenderTexture* CubemapRT[6];
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            CubemapRT[Idx] = Gfx->RequestRenderTexture("CubeCreationRT" + std::to_string(Idx), OutputExtent, UncompressedFormat);
        }

        // draw a pass that transfer sphere map to cube map
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            VkRenderPass SphereToCubemapRenderPass = Gfx->CreateRenderPass(UncompressedFormat, UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
            VkFramebuffer SphereToCubemapFrameBuffer = Gfx->CreateFrameBuffer(CubemapRT[Idx]->GetImageView(), SphereToCubemapRenderPass, OutputExtent);

            // setup shader
            UniquePtr<UHPanoramaToCubemapShader> SphereToCubemapShader = MakeUnique<UHPanoramaToCubemapShader>(Gfx, "SphereToCubemap", SphereToCubemapRenderPass);

            UniquePtr<UHRenderBuffer<int32_t>> ShaderData = Gfx->RequestRenderBuffer<int32_t>();
            ShaderData->CreateBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            ShaderData->UploadData(&Idx, 0);

            SphereToCubemapShader->BindConstant(ShaderData, 0, 0);
            SphereToCubemapShader->BindImage(InputTexture, 1);

            UHSamplerInfo Info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 1.0f);
            UHSampler* LinearWrappedSampler = Gfx->RequestTextureSampler(Info);
            SphereToCubemapShader->BindSampler(LinearWrappedSampler, 2);

            UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
            if (!InputTexture->HasUploadedToGPU())
            {
                InputTexture->UploadToGPU(Gfx, RenderBuilder);
            }

            RenderBuilder.BeginRenderPass(SphereToCubemapRenderPass, SphereToCubemapFrameBuffer, OutputExtent, VkClearValue{ {0,0,0,0} });
            RenderBuilder.SetViewport(OutputExtent);
            RenderBuilder.SetScissor(OutputExtent);

            // bind state
            RenderBuilder.BindGraphicState(SphereToCubemapShader->GetState());

            // bind sets
            RenderBuilder.BindDescriptorSet(SphereToCubemapShader->GetPipelineLayout(), SphereToCubemapShader->GetDescriptorSet(0));

            // draw fullscreen quad
            RenderBuilder.BindVertexBuffer(nullptr);
            RenderBuilder.DrawFullScreenQuad();
            RenderBuilder.EndRenderPass();
            Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

            UH_SAFE_RELEASE(SphereToCubemapShader);
            vkDestroyFramebuffer(Gfx->GetLogicalDevice(), SphereToCubemapFrameBuffer, nullptr);
            vkDestroyRenderPass(Gfx->GetLogicalDevice(), SphereToCubemapRenderPass, nullptr);
            UH_SAFE_RELEASE(ShaderData);
        }

        // Step 2 ------------------------------------------------- Copy from RT slices to regular slices
        Settings.bIsCompressed = false;
        Settings.CompressionSetting = CompressionNone;

        UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
        std::vector<UHTexture2D*> Slices(6);
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            // defer the mip map genertaion as copy texture doesn't work if mipmap count mismatched
            const std::string SliceName = "CubemapCreationSlice" + std::to_string(Idx);
            UHTexture2D Slice(SliceName, SliceName, OutputExtent, UncompressedFormat, Settings);
            Slices[Idx] = Gfx->RequestTexture2D(Slice, false);

            RenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            RenderBuilder.CopyTexture(CubemapRT[Idx], Slices[Idx]);
            RenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            Slices[Idx]->GenerateMipMaps(Gfx, RenderBuilder);
        }
        Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

        // now readback slices and recreate the slices following the input texture settings
        std::vector<uint8_t> SliceData[6];
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            SliceData[Idx] = Slices[Idx]->ReadbackTextureData();
            Slices[Idx]->SetTextureData(SliceData[Idx]);

            // recreate slices if compression is needed
            if (InputTexture->GetTextureSettings().CompressionSetting != CompressionNone)
            {
                Slices[Idx]->SetTextureSettings(InputTexture->GetTextureSettings());
                Slices[Idx]->Recreate(false);
            }
        }

        // Step 3 ------------------------------------------------- build a texture cube from slices
        RenderBuilder = UHRenderBuilder(Gfx, Gfx->BeginOneTimeCmd());

        const std::string CubeName = InputTexture->GetName() + "_Cube";
        if (UHTextureCube* OldCube = AssetMgr->GetCubemapByName(CubeName))
        {
            Gfx->RequestReleaseTextureCube(OldCube);
        }

        UHTextureCube* NewCube = Gfx->RequestTextureCube(CubeName, Slices);
        NewCube->Build(Gfx, RenderBuilder);

        Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

        // creation finished, export the cube
        std::string OutputPathName = OutputFolder.string() + "/" + NewCube->GetName();
        std::string SavedPathName = UHUtilities::StringReplace(OutputPathName, "\\", "/");
        SavedPathName = UHUtilities::StringReplace(SavedPathName, GTextureAssetFolder, "");
        NewCube->SetSourcePath(SavedPathName);
        NewCube->Export(GTextureAssetFolder + "/" + NewCube->GetSourcePath());

        CubemapDialog->OnCreationFinished(NewCube);

        // release all temporory textures
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            Slices[Idx]->ReleaseCPUTextureData();
            Gfx->RequestReleaseTexture2D(Slices[Idx]);
            Gfx->RequestReleaseRT(CubemapRT[Idx]);
        }
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

        UHStatusDialogScope StatusDialog("Creating...");

        // remove the existed one before creating new
        Gfx->WaitGPU();
        const std::string CubeName = Slices[0]->GetName() + "_Cube";
        if (UHTextureCube* OldCube = AssetMgr->GetCubemapByName(CubeName))
        {
            AssetMgr->RemoveCubemap(OldCube);
            Gfx->RequestReleaseTextureCube(OldCube);
        }

        UHTextureCube* NewCube = Gfx->RequestTextureCube(CubeName, Slices);
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
                NewCube->Build(Gfx, Builder);
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