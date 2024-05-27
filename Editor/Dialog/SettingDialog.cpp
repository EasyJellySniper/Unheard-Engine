#include "SettingDialog.h"

#if WITH_EDITOR
#include "Runtime/Engine/Config.h"
#include "Runtime/Engine/Engine.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"

UHSettingDialog::UHSettingDialog(UHConfigManager* InConfig, UHEngine* InEngine, UHDeferredShadingRenderer* InRenderer)
	: UHDialog(nullptr, nullptr)
    , Config(InConfig)
    , Engine(InEngine)
    , DeferredRenderer(InRenderer)
{

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
    static int32_t TempWidth = RenderingSettings.RenderWidth;
    static int32_t TempHeight = RenderingSettings.RenderHeight;
    ImGui::Text("---Rendering Settings---");
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
    if (ImGui::Checkbox("Enable RayTracing*", &RenderingSettings.bEnableRayTracing))
    {
        Engine->GetGfx()->WaitGPU();
        DeferredRenderer->UpdateDescriptors();
    }

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

    if (ImGui::Checkbox("Enable HDR", &RenderingSettings.bEnableHDR))
    {
        Engine->SetResizeReason(UHEngineResizeReason::ToggleHDR);
    }

    if (ImGui::Checkbox("Enable Hardware Occlusion", &RenderingSettings.bEnableHardwareOcclusion))
    {
        Engine->GetGfx()->WaitGPU();
        RenderingSettings.bEnableHardwareOcclusion ? DeferredRenderer->CreateOcclusionQuery() : DeferredRenderer->ReleaseOcclusionQuery();
    }
    ImGui::InputInt("Occlusion triangle threshold", &RenderingSettings.OcclusionTriangleThreshold);

    ImGui::InputInt("Parallel Threads (Up to 16)*", &RenderingSettings.ParallelThreads);
    ImGui::InputFloat("Final Reflection Strength", &RenderingSettings.FinalReflectionStrength);

    // raytracing settings
    ImGui::NewLine();
    ImGui::Text("---Raytracing Settings---");
    ImGui::InputFloat("RT Culling Distance", &RenderingSettings.RTCullingRadius);

    // RT Shadows
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

    // RT Reflections
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

    ImGui::PopItemWidth();
    bIsDialogActive |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();
}

#endif