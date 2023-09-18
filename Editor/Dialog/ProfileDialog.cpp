#include "ProfileDialog.h"

#if WITH_EDITOR
#include "../../resource.h"
#include "../Editor/Profiler.h"
#include "../../Runtime/Engine/Config.h"
#include <sstream>
#include <iomanip>

UHProfileDialog::UHProfileDialog()
    : UHDialog(nullptr, nullptr)
{

}

UHProfileDialog::UHProfileDialog(HINSTANCE InInstance, HWND InWindow)
	: UHDialog(InInstance, InWindow)
{

}

void UHProfileDialog::ShowDialog()
{
    if (!IsDialogActive(IDD_PROFILE))
    {
        Dialog = CreateDialog(Instance, MAKEINTRESOURCE(IDD_PROFILE), ParentWindow, (DLGPROC)GDialogProc);
        RegisterUniqueActiveDialog(IDD_PROFILE, this);
        CPUProfileLabel = UHLabel(GetDlgItem(Dialog, IDC_PROFILECPU), UHGUIProperty());
        GPUProfileLabel = UHLabel(GetDlgItem(Dialog, IDC_PROFILEGPU), UHGUIProperty());

        ShowWindow(Dialog, SW_SHOW);
    }
}

void UHProfileDialog::SyncProfileStatistics(UHProfiler* InProfiler, UHGameTimer* InGameTimer, UHConfigManager* InConfig)
{
    if (!IsDialogActive(IDD_PROFILE))
    {
        return;
    }

    // convert stats to string and display them
    const UHStatistics& Stats = InProfiler->GetStatistics();

    if (InGameTimer->GetTotalTime() > 1)
    {
        // CPU stat section
        std::wstringstream CPUStatTex;
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

        CPUProfileLabel.SetText(CPUStatTex.str());

        // GPU stat section
        std::wstring GPUStatStrings[UHRenderPassMax] = { L"Depth Pre Pass"
            , L"Base Pass"
            , L"Update Top Level AS"
            , L"Ray Tracing Shadow"
            , L"Light Culling"
            , L"Light Pass"
            , L"Skybox Pass"
            , L"Motion Pass"
            , L"Translucent Pass"
            , L"Tone Mapping"
            , L"Temporal AA"
            , L"Present SwapChain"};

        std::wstringstream GPUStatTex;
        GPUStatTex << "--GPU Profiles--\n";
        for (int32_t Idx = 0; Idx < UHRenderPassMax; Idx++)
        {
            GPUStatTex << GPUStatStrings[Idx] << ": " << std::fixed << std::setprecision(4)
                << InProfiler->GetStatistics().GPUTimes[Idx] << " ms\n";
        }
        GPUStatTex << "Render Resolution: " << InConfig->RenderingSetting().RenderWidth << "x" << InConfig->RenderingSetting().RenderHeight;

        GPUProfileLabel.SetText(GPUStatTex.str());

        InGameTimer->Reset();
    }
}

#endif