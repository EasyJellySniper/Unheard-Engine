#include "ProfileDialog.h"

#if WITH_DEBUG
#include "../../resource.h"
#include "../Profiler.h"
#include "../../Runtime/Engine/Config.h"
#include "../Classes/EditorUtils.h"
#include <sstream>
#include <iomanip>

HWND GProfileWindow = nullptr;

UHProfileDialog::UHProfileDialog()
    : UHDialog(nullptr, nullptr)
{

}

UHProfileDialog::UHProfileDialog(HINSTANCE InInstance, HWND InWindow)
	: UHDialog(InInstance, InWindow)
{

}


// Message handler for setting window
INT_PTR CALLBACK ProfileProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            GProfileWindow = nullptr;
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void UHProfileDialog::ShowDialog()
{
    if (GProfileWindow == nullptr)
    {
        GProfileWindow = CreateDialog(Instance, MAKEINTRESOURCE(IDD_PROFILE), Window, (DLGPROC)ProfileProc);
        ShowWindow(GProfileWindow, SW_SHOW);
    }
}

void UHProfileDialog::SyncProfileStatistics(UHProfiler* InProfiler, UHGameTimer* InGameTimer, UHConfigManager* InConfig)
{
    if (GProfileWindow == nullptr)
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

        UHEditorUtil::SetEditControl(GProfileWindow, IDC_PROFILECPU, CPUStatTex.str());

        // GPU stat section
        std::wstring GPUStatStrings[UHRenderPassMax] = { L"Update Top Level AS"
            , L"Ray Tracing Occlusion Test"
            , L"Depth Pre Pass"
            , L"Base Pass"
            , L"Ray Tracing Shadow"
            , L"Light Pass"
            , L"Skybox Pass"
            , L"Motion Pass"
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

        UHEditorUtil::SetEditControl(GProfileWindow, IDC_PROFILEGPU, GPUStatTex.str());

        InGameTimer->Reset();
    }
}

#endif