#include "SettingDialog.h"

#if WITH_DEBUG

#include "../EditorUI/EditorUtils.h"
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../resource.h"

HWND SettingWindow = nullptr;
WPARAM LatestControl = 0;

UHSettingDialog::UHSettingDialog()
	: UHSettingDialog(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
{
}

UHSettingDialog::UHSettingDialog(HINSTANCE InInstance, HWND InWindow, UHConfigManager* InConfig, UHEngine* InEngine
    , UHDeferredShadingRenderer* InRenderer, UHRawInput* InRawInput)
	: UHDialog(InInstance, InWindow)
    , Config(InConfig)
    , Engine(InEngine)
    , DeferredRenderer(InRenderer)
    , Input(InRawInput)
{
    // add callbacks
    ControlCallbacks[IDC_VSYNC] = { &UHSettingDialog::ControlVsync };
    ControlCallbacks[IDC_FULLSCREEN] = { &UHSettingDialog::ControlFullScreen };
    ControlCallbacks[IDC_CAMERASPEED] = { &UHSettingDialog::ControlCameraSpeed };
    ControlCallbacks[IDC_MOUSEROTATESPEED] = { &UHSettingDialog::ControlMouseRotateSpeed };
    ControlCallbacks[IDC_FORWARDKEY] = { &UHSettingDialog::ControlForwardKey };
    ControlCallbacks[IDC_BACKKEY] = { &UHSettingDialog::ControlBackKey };
    ControlCallbacks[IDC_LEFTKEY] = { &UHSettingDialog::ControlLeftKey };
    ControlCallbacks[IDC_RIGHTKEY] = { &UHSettingDialog::ControlRightKey };
    ControlCallbacks[IDC_DOWNKEY] = { &UHSettingDialog::ControlDownKey };
    ControlCallbacks[IDC_UPKEY] = { &UHSettingDialog::ControlUpKey };
    ControlCallbacks[IDC_FPSLIMIT] = { &UHSettingDialog::ControlFPSLimit };
    ControlCallbacks[IDC_MESHBUFFERMEMORYBUDGET] = { &UHSettingDialog::ControlBufferMemoryBudget };
    ControlCallbacks[IDC_IMAGEMEMORYBUDGET] = { &UHSettingDialog::ControlImageMemoryBudget };
    ControlCallbacks[IDC_APPLYRESOLUTION] = { &UHSettingDialog::ControlResolution };
    ControlCallbacks[IDC_ENABLETAA] = { &UHSettingDialog::ControlTAA };
    ControlCallbacks[IDC_ENABLERAYTRACING] = { &UHSettingDialog::ControlRayTracing };
    ControlCallbacks[IDC_ENABLERAYTRACINGOCCLUSIONTEST] = { &UHSettingDialog::ControlRayTracingOcclusionTest };
    ControlCallbacks[IDC_ENABLEGPULABELING] = { &UHSettingDialog::ControlGPULabeling };
    ControlCallbacks[IDC_ENABLELAYERVALIDATION] = { &UHSettingDialog::ControlLayerValidation };
    ControlCallbacks[IDC_ENABLEGPUTIMING] = { &UHSettingDialog::ControlGPUTiming };
    ControlCallbacks[IDC_ENABLEDEPTHPREPASS] = { &UHSettingDialog::ControlDepthPrePass };
    ControlCallbacks[IDC_ENABLEPARALLELSUBMISSION] = { &UHSettingDialog::ControlParallelSubmission };
    ControlCallbacks[IDC_PARALLELTHREADS] = { &UHSettingDialog::ControlParallelThread };
    ControlCallbacks[IDC_RTSHADOWQUALITY] = { &UHSettingDialog::ControlShadowQuality };
}

// Message handler for setting window
INT_PTR CALLBACK SettingProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        LatestControl = wParam;
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            SettingWindow = nullptr;
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void UHSettingDialog::ShowDialog()
{
    if (SettingWindow == nullptr)
    {
        SettingWindow = CreateDialog(Instance, MAKEINTRESOURCE(IDD_SETTING), Window, (DLGPROC)SettingProc);

        // sync ini settings to the window controls
        const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
        const UHEngineSettings& EngineSettings = Config->EngineSetting();
        const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

        // presentation
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_VSYNC, PresentSettings.bVsync);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_FULLSCREEN, PresentSettings.bFullScreen);

        // engine
        UHEditorUtil::SetEditControl(SettingWindow, IDC_CAMERASPEED, std::to_wstring((int)EngineSettings.DefaultCameraMoveSpeed));
        UHEditorUtil::SetEditControl(SettingWindow, IDC_MOUSEROTATESPEED, std::to_wstring((int)EngineSettings.MouseRotationSpeed));
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_FORWARDKEY, EngineSettings.ForwardKey);
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_BACKKEY, EngineSettings.BackKey);
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_LEFTKEY, EngineSettings.LeftKey);
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_RIGHTKEY, EngineSettings.RightKey);
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_DOWNKEY, EngineSettings.DownKey);
        UHEditorUtil::SetEditControlChar(SettingWindow, IDC_UPKEY, EngineSettings.UpKey);
        UHEditorUtil::SetEditControl(SettingWindow, IDC_FPSLIMIT, std::to_wstring((int)EngineSettings.FPSLimit));
        UHEditorUtil::SetEditControl(SettingWindow, IDC_MESHBUFFERMEMORYBUDGET, std::to_wstring((int)EngineSettings.MeshBufferMemoryBudgetMB));
        UHEditorUtil::SetEditControl(SettingWindow, IDC_IMAGEMEMORYBUDGET, std::to_wstring((int)EngineSettings.ImageMemoryBudgetMB));

        // rendering
        UHEditorUtil::SetEditControl(SettingWindow, IDC_RENDERWIDTH, std::to_wstring(RenderingSettings.RenderWidth));
        UHEditorUtil::SetEditControl(SettingWindow, IDC_RENDERHEIGHT, std::to_wstring(RenderingSettings.RenderHeight));
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLETAA, RenderingSettings.bTemporalAA);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLERAYTRACING, RenderingSettings.bEnableRayTracing);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLERAYTRACINGOCCLUSIONTEST, RenderingSettings.bEnableRayTracingOcclusionTest);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLEGPULABELING, RenderingSettings.bEnableGPULabeling);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLELAYERVALIDATION, RenderingSettings.bEnableLayerValidation);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLEGPUTIMING, RenderingSettings.bEnableGPUTiming);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLEPARALLELSUBMISSION, RenderingSettings.bParallelSubmission);
        UHEditorUtil::SetEditControl(SettingWindow, IDC_PARALLELTHREADS, std::to_wstring(RenderingSettings.ParallelThreads));
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLEDEPTHPREPASS, RenderingSettings.bEnableDepthPrePass);

        std::vector<std::wstring> ShadowQualities = { L"Full", L"Half", L"Quarter" };
        UHEditorUtil::InitComboBox(SettingWindow, IDC_RTSHADOWQUALITY, ShadowQualities[RenderingSettings.RTDirectionalShadowQuality], ShadowQualities);

        ShowWindow(SettingWindow, SW_SHOW);
    }
}

void UHSettingDialog::Update()
{
    if (!SettingWindow)
    {
        return;
    }

    if (LatestControl != 0)
    {
        if (ControlCallbacks.find(LOWORD(LatestControl)) != ControlCallbacks.end())
        {
            if (HIWORD(LatestControl) == EN_CHANGE || HIWORD(LatestControl) == BN_CLICKED
                || HIWORD(LatestControl) == CBN_SELCHANGE)
            {
                (this->*ControlCallbacks[LOWORD(LatestControl)])();
            }
        }

        LatestControl = 0;
    }

    // sync settings from input event
    const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
    const UHEngineSettings& EngineSettings = Config->EngineSetting();
    const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

    // full screen toggling
    if (Input->IsKeyHold(VK_MENU) && Input->IsKeyUp(VK_RETURN))
    {
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_FULLSCREEN, !PresentSettings.bFullScreen);
    }

    // TAA toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('t'))
    {
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLETAA, !RenderingSettings.bTemporalAA);
    }

    // vsync toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('v'))
    {
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_VSYNC, !PresentSettings.bVsync);
    }
}

void UHSettingDialog::ControlVsync()
{
    Config->ToggleVsync();
    Engine->SetResizeReason(UHEngineResizeReason::ToggleVsync);
}

void UHSettingDialog::ControlFullScreen()
{
    Engine->ToggleFullScreen();
}

void UHSettingDialog::ControlCameraSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DefaultCameraMoveSpeed = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_CAMERASPEED);
}

void UHSettingDialog::ControlMouseRotateSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MouseRotationSpeed = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_MOUSEROTATESPEED);
}

void UHSettingDialog::ControlForwardKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ForwardKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_FORWARDKEY);
}

void UHSettingDialog::ControlBackKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.BackKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_BACKKEY);
}

void UHSettingDialog::ControlLeftKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.LeftKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_LEFTKEY);
}

void UHSettingDialog::ControlRightKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.RightKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_RIGHTKEY);
}

void UHSettingDialog::ControlDownKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DownKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_DOWNKEY);
}

void UHSettingDialog::ControlUpKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.UpKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_UPKEY);
}

void UHSettingDialog::ControlFPSLimit()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.FPSLimit = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_FPSLIMIT);
}

void UHSettingDialog::ControlBufferMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MeshBufferMemoryBudgetMB = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_MESHBUFFERMEMORYBUDGET);
}

void UHSettingDialog::ControlImageMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ImageMemoryBudgetMB = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_IMAGEMEMORYBUDGET);
}

void UHSettingDialog::ControlResolution()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    int32_t Width = UHEditorUtil::GetEditControl<int32_t>(SettingWindow, IDC_RENDERWIDTH);
    int32_t Height = UHEditorUtil::GetEditControl<int32_t>(SettingWindow, IDC_RENDERHEIGHT);

    if (Width > 0 && Height > 0)
    {
        RenderingSettings.RenderWidth = Width;
        RenderingSettings.RenderHeight = Height;
        Engine->SetResizeReason(UHEngineResizeReason::NewResolution);
    }
}

void UHSettingDialog::ControlTAA()
{
    Config->ToggleTAA();
}

void UHSettingDialog::ControlRayTracing()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableRayTracing = !RenderingSettings.bEnableRayTracing;
}

void UHSettingDialog::ControlRayTracingOcclusionTest()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableRayTracingOcclusionTest = !RenderingSettings.bEnableRayTracingOcclusionTest;
}

void UHSettingDialog::ControlGPULabeling()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableGPULabeling = !RenderingSettings.bEnableGPULabeling;
}

void UHSettingDialog::ControlLayerValidation()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableLayerValidation = !RenderingSettings.bEnableLayerValidation;
}

void UHSettingDialog::ControlGPUTiming()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableGPUTiming = !RenderingSettings.bEnableGPUTiming;
    GEnableGPUTiming = RenderingSettings.bEnableGPUTiming;
}

void UHSettingDialog::ControlDepthPrePass()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableDepthPrePass = !RenderingSettings.bEnableDepthPrePass;
}

void UHSettingDialog::ControlParallelSubmission()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bParallelSubmission = !RenderingSettings.bParallelSubmission;
}

void UHSettingDialog::ControlParallelThread()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.ParallelThreads = UHEditorUtil::GetEditControl<int32_t>(SettingWindow, IDC_PARALLELTHREADS);
}

void UHSettingDialog::ControlShadowQuality()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.RTDirectionalShadowQuality = UHEditorUtil::GetComboBoxSelectedIndex(SettingWindow, IDC_RTSHADOWQUALITY);
    DeferredRenderer->ResizeRTBuffers();
}

#endif