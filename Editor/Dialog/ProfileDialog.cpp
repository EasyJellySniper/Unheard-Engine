#include "ProfileDialog.h"

#if WITH_EDITOR
#include "../Editor/Profiler.h"
#include "../../Runtime/Engine/Config.h"

UHProfileDialog::UHProfileDialog()
    : UHDialog(nullptr, nullptr)
{

}

void UHProfileDialog::SyncProfileStatistics(UHProfiler* InProfiler, UHGameTimer* InGameTimer, UHConfigManager* InConfig)
{
    if (!bIsOpened)
    {
        return;
    }

    // convert stats to string and display them
    const UHStatistics& Stats = InProfiler->GetStatistics();

    if (InGameTimer->GetTotalTime() > 1)
    {
        // CPU stat section
        CPUStatTex.str("");
        CPUStatTex.clear();
        CPUStatTex << "--CPU Profiles--\n";
        CPUStatTex << "Main Thread Time: " << std::fixed << std::setprecision(4) << Stats.MainThreadTime << " ms\n";
        CPUStatTex << "Render Thread Time: " << std::fixed << std::setprecision(4) << Stats.RenderThreadTime << " ms\n";
        CPUStatTex << "Total CPU Time: " << std::fixed << std::setprecision(4) << Stats.TotalTime << " ms\n";
        CPUStatTex << "FPS: " << std::setprecision(4) << Stats.FPS << "\n\n";
        
        CPUStatTex << "Number of draw calls: " << Stats.DrawCallCount << "\n";
        CPUStatTex << "Number of graphic states: " << Stats.PSOCount << "\n";
        CPUStatTex << "Shader Variants: " << Stats.ShaderCount << "\n";
        CPUStatTex << "Render Target in use: " << Stats.RTCount << "\n";
        CPUStatTex << "Number of texture samplers: " << Stats.SamplerCount << "\n";
        CPUStatTex << "Number of textures: " << Stats.TextureCount << "\n";
        CPUStatTex << "Number of texture cubes: " << Stats.TextureCubeCount << "\n";
        CPUStatTex << "Number of materials: " << Stats.MateralCount << "\n";

        // GPU stat section
        std::string GPUStatStrings[UHRenderPassMax] = { "Depth Pre Pass"
            , "Base Pass"
            , "Update Top Level AS"
            , "GenerateSH9"
            , "Ray Tracing Shadow"
            , "Light Culling"
            , "Light Pass"
            , "Skybox Pass"
            , "Motion Pass"
            , "Translucent Pass"
            , "Tone Mapping"
            , "Temporal AA"
            , "Present SwapChain"};

        GPUStatTex.str("");
        GPUStatTex.clear();
        GPUStatTex << "--GPU Profiles--\n";
        for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
        {
            GPUStatTex << GPUStatStrings[Idx] << ": " << std::fixed << std::setprecision(4)
                << InProfiler->GetStatistics().GPUTimes[Idx] << " ms\n";
        }
        GPUStatTex << "Render Resolution: " << InConfig->RenderingSetting().RenderWidth << "x" << InConfig->RenderingSetting().RenderHeight;

        InGameTimer->Reset();
    }

    // render GUI for stats
    ImGui::Begin("UHE Profile", &bIsOpened);
    ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "!!Disable layer validation for more accurate render thread/GPU time!!");

    if (ImGui::BeginTable("Profiles", 2))
    {
        ImGui::TableNextColumn();
        ImGui::Text(CPUStatTex.str().c_str());
        ImGui::TableNextColumn();
        ImGui::Text(GPUStatTex.str().c_str());
        ImGui::EndTable();
    }

    ImGui::End();
}

#endif