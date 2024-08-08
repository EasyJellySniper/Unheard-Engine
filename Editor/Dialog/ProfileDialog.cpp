#include "ProfileDialog.h"

#if WITH_EDITOR
#include "../Editor/Profiler.h"
#include "Runtime/Engine/Config.h"
#include "Runtime/Classes/GPUQuery.h"

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
    const auto& CPUScopeStats = UHGameTimerScope::GetResiteredGameTime();
    const auto& GPUScopeStats = UHGPUTimeQueryScope::GetResiteredGPUTime();

    static std::vector<std::stringstream> CPUTimeStats;

    if (InGameTimer->GetTotalTime() > 1.0f)
    {
        // CPU stats section
        CPUStatTex.str("");
        CPUStatTex.clear();
        CPUStatTex << "--CPU Profiles--\n";
        CPUStatTex << "Engine Update Time: " << std::fixed << std::setprecision(4) << Stats.EngineUpdateTime << " ms\n";
        CPUStatTex << "Render Thread Time: " << std::fixed << std::setprecision(4) << Stats.RenderThreadTime << " ms\n";
        CPUStatTex << "Total CPU Time: " << std::fixed << std::setprecision(4) << Stats.TotalTime << " ms\n";
        CPUStatTex << "FPS: " << std::setprecision(4) << Stats.FPS << "\n\n";

        // flush scoped time if there is any
        CPUTimeStats.clear();
        CPUTimeStats.resize(CPUScopeStats.size());
        for (size_t Idx = 0; Idx < CPUScopeStats.size(); Idx++)
        {
            CPUTimeStats[Idx] << "(" + std::to_string(Idx + 1) + ") " << CPUScopeStats[Idx].first << ": " << std::setprecision(4) << CPUScopeStats[Idx].second << " ms\n";
        }

        // other CPU stats
        CPUStatTex << "--Misc CPU stats--\n";
        CPUStatTex << "Number of total renderers: " << Stats.RendererCount << "\n";
        CPUStatTex << "Number of draw calls: " << Stats.DrawCallCount << "\n";
        CPUStatTex << "Number of occlusion tests: " << Stats.OccludedCallCount << "\n";
        CPUStatTex << "Number of graphic states: " << Stats.PSOCount << "\n";
        CPUStatTex << "Shader Variants: " << Stats.ShaderCount << "\n";
        CPUStatTex << "Render Target in use: " << Stats.RTCount << "\n";
        CPUStatTex << "Number of texture samplers: " << Stats.SamplerCount << "\n";
        CPUStatTex << "Number of textures: " << Stats.TextureCount << "\n";
        CPUStatTex << "Number of texture cubes: " << Stats.TextureCubeCount << "\n";
        CPUStatTex << "Number of materials: " << Stats.MateralCount << "\n";
        CPUStatTex << std::endl;

        // GPU stat section
        float TotalGPUTime = 0.0f;
        GPUStatTex.str("");
        GPUStatTex.clear();
        GPUStatTex << "--GPU Profiles--\n";

        // iterating all registered GPU time
        for (size_t Idx = 0; Idx < GPUScopeStats.size(); Idx++)
        {
            GPUStatTex << GPUScopeStats[Idx]->GetDebugName() << ": " << std::fixed << std::setprecision(4)
                << GPUScopeStats[Idx]->GetLastTimeStamp() << " ms\n";

            TotalGPUTime += GPUScopeStats[Idx]->GetLastTimeStamp();
        }

        GPUStatTex << "Total GPU Time: " << TotalGPUTime << " ms\n";
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

        // print time-based cpu stat after regular stat
        const auto& CPUScopeStats = UHGameTimerScope::GetResiteredGameTime();
        for (size_t Idx = 0; Idx < CPUScopeStats.size(); Idx++)
        {
            ImVec4 CPUTimeStatColor;
            if (CPUScopeStats[Idx].second <= 0.1)
            {
                CPUTimeStatColor = ImVec4(1, 1, 1, 1);
            }
            else if (CPUScopeStats[Idx].second > 0.1)
            {
                CPUTimeStatColor = ImVec4(1, 1, 0, 1);
            }
            else if (CPUScopeStats[Idx].second > 5)
            {
                CPUTimeStatColor = ImVec4(1, 0.6, 0, 1);
            }
            else
            {
                CPUTimeStatColor = ImVec4(1, 0, 0, 1);
            }
            ImGui::TextColored(CPUTimeStatColor, CPUTimeStats[Idx].str().c_str());
        }

        ImGui::TableNextColumn();
        ImGui::Text(GPUStatTex.str().c_str());
        ImGui::EndTable();
    }

    ImGui::End();
}

#endif