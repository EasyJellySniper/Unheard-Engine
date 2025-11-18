#include "SettingDialog.h"

#if WITH_EDITOR
#include "Runtime/Engine/Config.h"
#include "Runtime/Engine/Engine.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"

int32_t UHSettingDialog::TempWidth;
int32_t UHSettingDialog::TempHeight;

UHSettingDialog::UHSettingDialog(UHConfigManager* InConfig, UHEngine* InEngine, UHDeferredShadingRenderer* InRenderer)
	: UHDialog(nullptr, nullptr)
    , Config(InConfig)
    , Engine(InEngine)
    , DeferredRenderer(InRenderer)
{

}

void UHSettingDialog::ShowDialog()
{
    UHDialog::ShowDialog();

    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    TempWidth = RenderingSettings.RenderWidth;
    TempHeight = RenderingSettings.RenderHeight;
}

void UHSettingDialog::Update(bool& bIsDialogActive)
{
    if (!bIsOpened)
    {
        return;
    }

    // sync settings from input event
    UHPresentationSettings& PresentSettings = Config->PresentationSetting();
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

    // draw ImGui
    ImGui::Begin("UHE Settings", &bIsOpened);
    ImGui::PushItemWidth(100.0f);
    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "All settings end with star(*) need a relaunch.");

    // presentation settings
    ImGui::Text("---Presentation Settings---");

    if (ImGui::Checkbox("Enable VSync (Ctrl + V)", &PresentSettings.bVsync))
    {
        Engine->SetResizeReason(UHEngineResizeReason::ToggleVsync);
    }

    if (ImGui::Checkbox("Full screen (Alt + Enter)", &PresentSettings.bFullScreen))
    {
        Engine->ToggleFullScreen();
    }
    ImGui::NewLine();

    // engine settings
    ImGui::Text("---Engine Settings---");
    ImGui::InputFloat("DefaultCameraMoveSpeed", &EngineSettings.DefaultCameraMoveSpeed);
    ImGui::InputFloat("MouseRotationSpeed", &EngineSettings.MouseRotationSpeed);

    ImGui::InputChar("ForwardKey", &EngineSettings.ForwardKey);
    ImGui::InputChar("BackKey", &EngineSettings.BackKey);
    ImGui::InputChar("LeftKey", &EngineSettings.LeftKey);
    ImGui::InputChar("RightKey", &EngineSettings.RightKey);
    ImGui::InputChar("DownKey", &EngineSettings.DownKey);
    ImGui::InputChar("UpKey", &EngineSettings.UpKey);

    ImGui::InputFloat("FPSLimit", &EngineSettings.FPSLimit);
    ImGui::InputFloat("MeshBufferMemoryBudgetMB*", &EngineSettings.MeshBufferMemoryBudgetMB);
    ImGui::InputFloat("ImageMemoryBudgetMB*", &EngineSettings.ImageMemoryBudgetMB);
    ImGui::NewLine();

    // rendering settings
    ImGui::Text("---Rendering Settings---");

    // selected GPU setting
    char Dummy[40];
    memset(Dummy, 65, sizeof(char) * 40);
    Dummy[39] = '\0';

    // give a good size for combo list display
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(Dummy).x);
    if (ImGui::BeginCombo("Selected GPU*", RenderingSettings.SelectedGpuName.c_str()))
    {
        const std::vector<std::string>& AvailableGpus = Engine->GetGfx()->GetAvailableGpuNames();
        for (size_t Idx = 0; Idx < AvailableGpus.size(); Idx++)
        {
            const bool bIsSelected = (RenderingSettings.SelectedGpuName == AvailableGpus[Idx]);
            if (ImGui::Selectable(AvailableGpus[Idx].c_str(), bIsSelected))
            {
                RenderingSettings.SelectedGpuName = AvailableGpus[Idx];
                break;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::NewLine();

    ImGui::InputInt("##", &TempWidth);
    ImGui::SameLine();
    ImGui::InputInt("Render Resolution (W/H)", &TempHeight);
    ImGui::SameLine();
    if (ImGui::Button("Apply"))
    {
        RenderingSettings.RenderWidth = TempWidth;
        RenderingSettings.RenderHeight = TempHeight;
        Engine->SetResizeReason(UHEngineResizeReason::NewResolution);
    }

    ImGui::Checkbox("Enable TAA", &RenderingSettings.bTemporalAA);
    ImGui::Checkbox("Enable GPU Labeling (For RenderDoc debugging)", &RenderingSettings.bEnableGPULabeling);
    ImGui::Checkbox("Enable Layer Validation*", &RenderingSettings.bEnableLayerValidation);
    if (ImGui::Checkbox("Enable GPU Timing", &RenderingSettings.bEnableGPUTiming))
    {
        GEnableGPUTiming = RenderingSettings.bEnableGPUTiming;
    }

    if (ImGui::Checkbox("Enable Depth Pre Pass", &RenderingSettings.bEnableDepthPrePass))
    {
        Engine->GetGfx()->WaitGPU();
        Engine->GetGfx()->SetDepthPrepassActive(RenderingSettings.bEnableDepthPrePass);
        DeferredRenderer->ToggleDepthPrepass();
    }

    if (ImGui::Checkbox("Enable Async Compute", &RenderingSettings.bEnableAsyncCompute))
    {
        Engine->GetGfx()->WaitGPU();
        RenderingSettings.bEnableAsyncCompute ? (void)DeferredRenderer->CreateAsyncComputeQueue() : DeferredRenderer->ReleaseAsyncComputeQueue();
    }

    ImGui::Checkbox("Enable Hardware Occlusion", &RenderingSettings.bEnableHardwareOcclusion);
    ImGui::InputInt("Occlusion triangle threshold", &RenderingSettings.OcclusionTriangleThreshold);

    ImGui::InputInt("Parallel Render Submitters (Up to 8)*", &RenderingSettings.ParallelSubmitters);
    ImGui::InputFloat("Final Reflection Strength", &RenderingSettings.FinalReflectionStrength);

    // Common Gamma setting
    ImGui::NewLine();
    ImGui::Text("Non-HDR gamma correction");
    ImGui::SliderFloat("Gamma Correction", &RenderingSettings.GammaCorrection, 0.5f, 5.0f, "%.1f");

    // HDR settings
    ImGui::NewLine();
    ImGui::Text("---HDR Settings---");
    if (ImGui::Checkbox("Enable HDR", &RenderingSettings.bEnableHDR))
    {
        Engine->SetResizeReason(UHEngineResizeReason::ToggleHDR);
    }

    if (ImGui::SliderFloat("HDR Whitepaper Nits", &RenderingSettings.HDRWhitePaperNits, 100.0f, 1000.0f, "%.1f"))
    {
        // align to a multiple of 10
        RenderingSettings.HDRWhitePaperNits = MathHelpers::RoundUpToMultiple(RenderingSettings.HDRWhitePaperNits, 10.0f);
    }

    ImGui::SliderFloat("HDR Contrast", &RenderingSettings.HDRContrast, 0.5f, 2.5f, "%.1f");

    // soft shadow settings
    ImGui::NewLine();
    ImGui::Text("---Soft shadow Settings---");
    if (ImGui::InputInt("PCSS Kernal", &RenderingSettings.PCSSKernal))
    {
        RenderingSettings.PCSSKernal = std::clamp(RenderingSettings.PCSSKernal, 1, 3);
    }

    if (ImGui::InputFloat("PCSS Min Penumbra", &RenderingSettings.PCSSMinPenumbra))
    {
        RenderingSettings.PCSSMinPenumbra = std::max(RenderingSettings.PCSSMinPenumbra, 0.0f);
    }

    if (ImGui::InputFloat("PCSS Max Penumbra", &RenderingSettings.PCSSMaxPenumbra))
    {
        RenderingSettings.PCSSMaxPenumbra = std::max(RenderingSettings.PCSSMaxPenumbra, 0.0f);
    }

    if (ImGui::InputFloat("PCSS Blocker Dist Scale", &RenderingSettings.PCSSBlockerDistScale))
    {
        RenderingSettings.PCSSBlockerDistScale = std::max(RenderingSettings.PCSSBlockerDistScale, 0.0f);
    }

    // raytracing settings
    ImGui::NewLine();
    ImGui::Text("---Raytracing Settings---");
    if (ImGui::Checkbox("Enable RayTracing*", &RenderingSettings.bEnableRayTracing))
    {
        Engine->GetGfx()->WaitGPU();
        DeferredRenderer->UpdateDescriptors();
    }

    ImGui::InputFloat("RT Culling Distance", &RenderingSettings.RTCullingRadius);
    ImGui::Checkbox("Ray Tracing Denoise", &RenderingSettings.bDenoiseRayTracing);
    ImGui::NewLine();

    // RT Shadows
    if (ImGui::Checkbox("Enable RT Shadow", &RenderingSettings.bEnableRTShadow))
    {
        Engine->GetGfx()->WaitGPU();
        DeferredRenderer->UpdateDescriptors();
    }

    if (RenderingSettings.bEnableRTShadow)
    {
        std::vector<std::string> RTShadowQualities = { "Full", "Half" };
        if (ImGui::BeginCombo("Ray Tracing Shadow Quaility", RTShadowQualities[RenderingSettings.RTShadowQuality].c_str()))
        {
            for (size_t Idx = 0; Idx < RTShadowQualities.size(); Idx++)
            {
                const bool bIsSelected = (RenderingSettings.RTShadowQuality == Idx);
                if (ImGui::Selectable(RTShadowQualities[Idx].c_str(), bIsSelected))
                {
                    RenderingSettings.RTShadowQuality = static_cast<int32_t>(Idx);
                    DeferredRenderer->ResizeRayTracingBuffers(false);
                    break;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputFloat("RT Shadow TMax", &RenderingSettings.RTShadowTMax);
    }
    ImGui::NewLine();

    // RT Reflections
    if (ImGui::Checkbox("Enable RT Reflection", &RenderingSettings.bEnableRTReflection))
    {
        Engine->GetGfx()->WaitGPU();
        DeferredRenderer->UpdateDescriptors();
    }

    if (RenderingSettings.bEnableRTReflection)
    {
        std::vector<std::string> RTReflectionQualities = { "Full", "Half Pixel", "Quarter Pixel" };
        if (ImGui::BeginCombo("Ray Tracing Reflection Quaility", RTReflectionQualities[RenderingSettings.RTReflectionQuality].c_str()))
        {
            for (size_t Idx = 0; Idx < RTReflectionQualities.size(); Idx++)
            {
                const bool bIsSelected = (RenderingSettings.RTReflectionQuality == Idx);
                if (ImGui::Selectable(RTReflectionQualities[Idx].c_str(), bIsSelected))
                {
                    RenderingSettings.RTReflectionQuality = static_cast<int32_t>(Idx);
                    DeferredRenderer->ResizeRayTracingBuffers(false);
                    break;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputFloat("RT Reflection TMax", &RenderingSettings.RTReflectionTMax);
        ImGui::InputFloat("RT Reflection Smooth Cutoff", &RenderingSettings.RTReflectionSmoothCutoff);
    }
    ImGui::NewLine();

    // RT Indirect lighting
    ImGui::Checkbox("Enable RT Indirect Lighting", &RenderingSettings.bEnableRTIndirectLighting);

    ImGui::PopItemWidth();
    bIsDialogActive |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();
}

#endif