#include "TextureCreationDialog.h"

#if WITH_EDITOR
#include "../../resource.h"
#include "../Classes/EditorUtils.h"
#include "Runtime/Classes/Utility.h"
#include <filesystem>
#include "TextureDialog.h"
#include "CubemapDialog.h"
#include "Runtime/Classes/AssetPath.h"
#include "Runtime/Engine/Asset.h"
#include "Runtime/Engine/Graphic.h"
#include "Runtime/Renderer/RenderBuilder.h"
#include "Runtime/Renderer/RendererShared.h"
#include "StatusDialog.h"
#include "Runtime/Renderer/ShaderClass/PanoramaToCubemapShader.h"
#include "Runtime/Renderer/ShaderClass/SmoothCubemapShader.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"

struct UHPanoramaData
{
    int32_t FaceIndex;
    int32_t MipIndex;
};

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
    CurrentOutputPath = "";
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
    CurrentOutputPath = "";
}

void UHTextureCreationDialog::Update(bool& bIsDialogActive)
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

        std::string CompressionModeTextA = UHUtilities::ToStringA(GCompressionModeText[UH_ENUM_VALUE(CurrentEditingSettings.CompressionSetting)]);
        if (ImGui::BeginCombo("Compression Mode", CompressionModeTextA.c_str()))
        {
            for (size_t Idx = 0; Idx < GCompressionModeText.size(); Idx++)
            {
                const bool bIsSelected = (UH_ENUM_VALUE(CurrentEditingSettings.CompressionSetting) == Idx);
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

        ImGui::Text("Input Path (for multiple textures import):");
        ImGui::Text(CurrentSourceFolder.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse Path"))
        {
            ControlBrowserInputPath();
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

bool IsSupportedImageFormat(std::string InExtension)
{
    for (int32_t Idx = 0; Idx < GNumMaxSupportedImageFormat; Idx++)
    {
        if (GSupportedImageFormat[Idx] ==  UHUtilities::ToLowerString(InExtension))
        {
            return true;
        }
    }

    return false;
}

void UHTextureCreationDialog::ControlTextureCreate()
{
    const std::filesystem::path InputSource = CurrentSourceFile;
    const std::filesystem::path InputFolder = CurrentSourceFolder;
    std::filesystem::path OutputFolder = CurrentOutputPath;
    std::filesystem::path TextureAssetPath = GTextureAssetFolder;
    TextureAssetPath = std::filesystem::absolute(TextureAssetPath);

    bool bIsValidOutputFolder = false;
    bIsValidOutputFolder |= UHUtilities::StringFind(OutputFolder.string() + "\\", TextureAssetPath.string());
    bIsValidOutputFolder |= UHUtilities::StringFind(TextureAssetPath.string(), OutputFolder.string() + "\\");

    bool bSingleInputSource = std::filesystem::exists(InputSource);
    bool bMultipleInputSources = std::filesystem::exists(InputFolder);

    if (!bSingleInputSource && !bMultipleInputSources)
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

    if (bSingleInputSource)
    {
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

    if (bMultipleInputSources)
    {
        // iterate all image files under the input path
        std::vector<UHTexture*> NewTexes;
        for (std::filesystem::recursive_directory_iterator Idx(InputFolder), end; Idx != end; Idx++)
        {
            if (std::filesystem::is_directory(Idx->path()) || !IsSupportedImageFormat(Idx->path().extension().string()))
            {
                continue;
            }
            
            std::filesystem::path RawSourcePath = std::filesystem::relative(Idx->path());
            if (RawSourcePath.string().empty())
            {
                RawSourcePath = Idx->path();
            }

            UHTextureSettings TextureSetting = CurrentEditingSettings;
            TextureSetting.bIsHDR = (RawSourcePath.extension().string() == ".exr");
            ValidateTextureSetting(TextureSetting);

            UHTexture* NewTex = TextureImporter->ImportRawTexture(RawSourcePath, OutputFolder, TextureSetting, true);
            if (NewTex)
            {
                NewTexes.push_back(NewTex);
            }
        }

        TextureDialog->OnCreationFinished(NewTexes);
        MessageBoxA(nullptr, "Multiple Texture import successfully.", "Texture Import", MB_OK);
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

    // temporary resource defines
    UHRenderTexture* CubemapRT[6];
    std::vector<UHTexture2D*> Slices(6);

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
        UHTextureFormat UncompressedFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;
        if (Settings.bIsHDR)
        {
            UncompressedFormat = UHTextureFormat::UH_FORMAT_RGBA16F;
        }

        // Step 1 ------------------------------------------------- Convert panorama to individual cube slice RT
        UHRenderTextureSettings RenderTextureSettings{};
        RenderTextureSettings.bIsReadWrite = true;
        RenderTextureSettings.bUseMipmap = true;
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            CubemapRT[Idx] = Gfx->RequestRenderTexture("CubeCreationRT" + std::to_string(Idx), OutputExtent, UncompressedFormat, RenderTextureSettings);
        }

        // draw a pass that transfer sphere map to cube map, do this for all mipmaps
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            for (uint32_t MipIdx = 0; MipIdx < CubemapRT[Idx]->GetMipMapCount(); MipIdx++)
            {
                // adjust mip extent
                VkExtent2D MipExtent = OutputExtent;
                MipExtent.width >>= MipIdx;
                MipExtent.height >>= MipIdx;

                UHRenderPassObject SphereToCubemapRenderPass = Gfx->CreateRenderPass(CubemapRT[Idx], UHTransitionInfo(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_GENERAL));
                VkFramebuffer SphereToCubemapFrameBuffer = Gfx->CreateFrameBuffer(CubemapRT[Idx], SphereToCubemapRenderPass.RenderPass, MipExtent);
                SphereToCubemapRenderPass.FrameBuffer = SphereToCubemapFrameBuffer;

                // setup shader
                UniquePtr<UHPanoramaToCubemapShader> SphereToCubemapShader = MakeUnique<UHPanoramaToCubemapShader>(Gfx, "SphereToCubemap", SphereToCubemapRenderPass.RenderPass);

                // setup uniform data
                UHPanoramaData Data{};
                Data.FaceIndex = Idx;
                Data.MipIndex = MipIdx;

                UniquePtr<UHRenderBuffer<UHPanoramaData>> ShaderData = Gfx->RequestRenderBuffer<UHPanoramaData>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                    , "PanoramaData");
                ShaderData->UploadData(&Data, 0);

                // non biased sampler
                UHSamplerInfo SamplerInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1.0f);
                SamplerInfo.MipBias = 0.0f;
                UHSampler* LinearClampedSampler = Gfx->RequestTextureSampler(SamplerInfo);

                SphereToCubemapShader->BindConstant(ShaderData, 0, 0, 0);
                SphereToCubemapShader->BindImage(InputTexture, 1);
                SphereToCubemapShader->BindSampler(LinearClampedSampler, 2);

                UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
                if (!InputTexture->HasUploadedToGPU())
                {
                    InputTexture->UploadToGPU(Gfx, RenderBuilder);
                }

                RenderBuilder.BeginRenderPass(SphereToCubemapRenderPass, MipExtent, VkClearValue{ {0,0,0,0} });
                RenderBuilder.SetViewport(MipExtent);
                RenderBuilder.SetScissor(MipExtent);

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
                vkDestroyRenderPass(Gfx->GetLogicalDevice(), SphereToCubemapRenderPass.RenderPass, nullptr);
                UH_SAFE_RELEASE(ShaderData);
            }
        }

        // Step 2 ------------------------------------------------- Smooth the cubemap edge
        UniquePtr<UHSmoothCubemapShader> SmoothCubemap = MakeUnique<UHSmoothCubemapShader>(Gfx, "SmoothCubemapShader");
        for (uint32_t MipIdx = 0; MipIdx < CubemapRT[0]->GetMipMapCount(); MipIdx++)
        {
            UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
            UniquePtr<UHRenderBuffer<uint32_t>> ShaderData = Gfx->RequestRenderBuffer<uint32_t>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                , "SmoothCubeShaderData");

            // smooth shader
            for (int32_t Idx = 0; Idx < 6; Idx++)
            {
                SmoothCubemap->BindRWImage(CubemapRT[Idx], Idx, MipIdx);
            }

            uint32_t MipSize = Size >> MipIdx;
            ShaderData->UploadData(&MipSize, 0);
            SmoothCubemap->BindConstant(ShaderData, 6, 0, 0);

            RenderBuilder.BindComputeState(SmoothCubemap->GetComputeState());
            RenderBuilder.BindDescriptorSetCompute(SmoothCubemap->GetPipelineLayout(), SmoothCubemap->GetDescriptorSet(0));
            RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(GThreadGroup2D_X, Size), 1, 1);

            Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());
            UH_SAFE_RELEASE(ShaderData);
        }
        UH_SAFE_RELEASE(SmoothCubemap);

        // Step 3 ------------------------------------------------- Copy from RT slices to regular slices
        Settings.bIsCompressed = false;
        Settings.CompressionSetting = UHTextureCompressionSettings::CompressionNone;

        UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            const std::string SliceName = "CubemapCreationSlice" + std::to_string(Idx);
            UniquePtr<UHTexture2D> Slice = MakeUnique<UHTexture2D>(SliceName, SliceName, OutputExtent, UncompressedFormat, Settings);
            Slices[Idx] = Gfx->RequestTexture2D(Slice, false);

            for (uint32_t MipIdx = 0; MipIdx < Slices[Idx]->GetMipMapCount(); MipIdx++)
            {
                RenderBuilder.PushResourceBarrier(UHImageBarrier(CubemapRT[Idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, MipIdx));
                RenderBuilder.PushResourceBarrier(UHImageBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipIdx));
                RenderBuilder.FlushResourceBarrier();
                RenderBuilder.CopyTexture(CubemapRT[Idx], Slices[Idx], MipIdx);
                RenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, MipIdx);
            }
        }
        Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

        // now readback slices and recreate the slices following the input texture settings
        std::vector<uint8_t> SliceData[6];
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            SliceData[Idx] = Slices[Idx]->ReadbackTextureData();

            // bypass gpu uploading and mipmap generation
            Slices[Idx]->SetHasUploadedToGPU(true);
            Slices[Idx]->SetMipmapGenerated(true);

            // recreate slices if compression is needed
            if (InputTexture->GetTextureSettings().CompressionSetting != UHTextureCompressionSettings::CompressionNone)
            {
                Slices[Idx]->SetTextureSettings(InputTexture->GetTextureSettings());
                Slices[Idx]->Recreate(false, SliceData[Idx]);
            }
        }

        // Step 4 ------------------------------------------------- build a texture cube from slices
        RenderBuilder = UHRenderBuilder(Gfx, Gfx->BeginOneTimeCmd());

        const std::string CubeName = InputTexture->GetName() + "_Cube";
        UHTextureCube* OldCube = AssetMgr->GetCubemapByName(CubeName);
        UHTextureCube* NewCube = (OldCube) ? OldCube : Gfx->RequestTextureCube(CubeName, Slices);
        if (OldCube)
        {
            NewCube->SetTextureSettings(Slices[0]->GetTextureSettings());
            NewCube->SetSlices(Slices);
            NewCube->Recreate(Slices[0]->GetFormat());
        }
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
        UHRenderBuilder RenderBuilder = UHRenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
        uint32_t Size = Slices[0]->GetExtent().width;

        // Step 1 ------------------------------------------------- Blit to cubemap RT for smoothing the cube edge
        UHTextureSettings Settings = Slices[0]->GetTextureSettings();
        UHTextureFormat UncompressedFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;
        if (Settings.bIsHDR)
        {
            UncompressedFormat = UHTextureFormat::UH_FORMAT_RGBA16F;
        }

        UHRenderTextureSettings RenderTextureSettings{};
        RenderTextureSettings.bIsReadWrite = true;
        RenderTextureSettings.bUseMipmap = true;
        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            CubemapRT[Idx] = Gfx->RequestRenderTexture("CubeCreationRT" + std::to_string(Idx), Slices[Idx]->GetExtent(), UncompressedFormat, RenderTextureSettings);
            if (!Slices[Idx]->HasUploadedToGPU())
            {
                Slices[Idx]->UploadToGPU(Gfx, RenderBuilder);
            }

            for (uint32_t MipIdx = 0; MipIdx < Slices[Idx]->GetMipMapCount(); MipIdx++)
            {
                VkExtent2D MipExtent = Slices[Idx]->GetExtent();
                MipExtent.width >>= MipIdx;
                MipExtent.height >>= MipIdx;

                RenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, MipIdx);
                RenderBuilder.ResourceBarrier(CubemapRT[Idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipIdx);
                RenderBuilder.Blit(Slices[Idx], CubemapRT[Idx], MipExtent, MipExtent, MipIdx, MipIdx, VK_FILTER_LINEAR);
                RenderBuilder.ResourceBarrier(CubemapRT[Idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, MipIdx);
                RenderBuilder.ResourceBarrier(Slices[Idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, MipIdx);
            }
        }
        Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

        // Step 2 ------------------------------------------------- Smooth the edge of cubemap
        UniquePtr<UHSmoothCubemapShader> SmoothCubemap = MakeUnique<UHSmoothCubemapShader>(Gfx, "SmoothCubemapShader");
        for (uint32_t MipIdx = 0; MipIdx < CubemapRT[0]->GetMipMapCount(); MipIdx++)
        {
            UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());

            // smooth shader
            for (int32_t Idx = 0; Idx < 6; Idx++)
            {
                SmoothCubemap->BindRWImage(CubemapRT[Idx], Idx, MipIdx);
            }
            UniquePtr<UHRenderBuffer<uint32_t>> ShaderData = Gfx->RequestRenderBuffer<uint32_t>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                , "SmoothCubeShaderData");
            uint32_t MipSize = Size >> MipIdx;
            ShaderData->UploadData(&MipSize, 0);
            SmoothCubemap->BindConstant(ShaderData, 6, 0, 0);

            RenderBuilder.BindComputeState(SmoothCubemap->GetComputeState());
            RenderBuilder.BindDescriptorSetCompute(SmoothCubemap->GetPipelineLayout(), SmoothCubemap->GetDescriptorSet(0));
            RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(GThreadGroup2D_X, Size), 1, 1);

            Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());
            UH_SAFE_RELEASE(ShaderData);
        }
        UH_SAFE_RELEASE(SmoothCubemap);

        // Step 3 ------------------------------------------------- readback slice data and compress again
        const std::string CubeName = Slices[0]->GetName() + "_Cube";

        for (int32_t Idx = 0; Idx < 6; Idx++)
        {
            UHRenderBuilder RenderBuilder(Gfx, Gfx->BeginOneTimeCmd());
            for (uint32_t MipIdx = 0; MipIdx < CubemapRT[Idx]->GetMipMapCount(); MipIdx++)
            {
                RenderBuilder.ResourceBarrier(CubemapRT[Idx], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, MipIdx);
            }
            Gfx->EndOneTimeCmd(RenderBuilder.GetCmdList());

            // request new slice and recreate with readback data
            CubemapRT[Idx]->SetGfxCache(Gfx);
            const std::vector<uint8_t>& Data = CubemapRT[Idx]->ReadbackTextureData();
            const std::string SliceName = "CubemapCreationSlice" + std::to_string(Idx);
            UniquePtr<UHTexture2D> Slice = MakeUnique<UHTexture2D>(SliceName, SliceName, Slices[Idx]->GetExtent(), Slices[Idx]->GetFormat(), Settings);
            Slices[Idx] = Gfx->RequestTexture2D(Slice, false);
            Slices[Idx]->Recreate(false, Data);
        }

        // Step 4 ------------------------------------------------- Recreate cubemap
        UHTextureCube* OldCube = AssetMgr->GetCubemapByName(CubeName);
        UHTextureCube* NewCube = (OldCube) ? OldCube : Gfx->RequestTextureCube(CubeName, Slices);
        if (OldCube)
        {
            NewCube->SetTextureSettings(Slices[0]->GetTextureSettings());
            NewCube->SetSlices(Slices);
            NewCube->Recreate(Slices[0]->GetFormat());
        }

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

    // release all temporory textures
    for (int32_t Idx = 0; Idx < 6; Idx++)
    {
        Slices[Idx]->ReleaseCPUTextureData();
        Gfx->RequestReleaseTexture2D(Slices[Idx]);
        Gfx->RequestReleaseRT(CubemapRT[Idx]);
    }
}

void UHTextureCreationDialog::ControlBrowserInput()
{
    std::filesystem::path DefaultPath = UHUtilities::ToStringW(GRawTextureAssetPath);
    DefaultPath = std::filesystem::absolute(DefaultPath);
    CurrentSourceFile = UHUtilities::ToStringA(UHEditorUtil::FileSelectInput(GImageFilter, DefaultPath.wstring()));
}

void UHTextureCreationDialog::ControlBrowserInputPath()
{
    std::filesystem::path DefaultPath = UHUtilities::ToStringW(GRawTextureAssetPath);
    DefaultPath = std::filesystem::absolute(DefaultPath);
    CurrentSourceFolder = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder(DefaultPath.wstring()));
}

void UHTextureCreationDialog::ControlBrowserOutputFolder()
{
    std::filesystem::path DefaultPath = UHUtilities::ToStringW(GTextureAssetFolder);
    DefaultPath = std::filesystem::absolute(DefaultPath);
    CurrentOutputPath = UHUtilities::ToStringA(UHEditorUtil::FileSelectOutputFolder(DefaultPath.wstring()));
}

#endif