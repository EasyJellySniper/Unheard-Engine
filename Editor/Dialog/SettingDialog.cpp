#include "SettingDialog.h"

#if WITH_DEBUG

#include "../Classes/EditorUtils.h"
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../resource.h"

HWND GSettingWindow = nullptr;
WPARAM GLatestControl = 0;

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
    ControlCallbacks[IDC_ENABLEGPULABELING] = { &UHSettingDialog::ControlGPULabeling };
    ControlCallbacks[IDC_ENABLELAYERVALIDATION] = { &UHSettingDialog::ControlLayerValidation };
    ControlCallbacks[IDC_ENABLEGPUTIMING] = { &UHSettingDialog::ControlGPUTiming };
    ControlCallbacks[IDC_ENABLEDEPTHPREPASS] = { &UHSettingDialog::ControlDepthPrePass };
    ControlCallbacks[IDC_ENABLEPARALLELSUBMISSION] = { &UHSettingDialog::ControlParallelSubmission };
    ControlCallbacks[IDC_PARALLELTHREADS] = { &UHSettingDialog::ControlParallelThread };
    ControlCallbacks[IDC_RTSHADOWQUALITY] = { &UHSettingDialog::ControlShadowQuality };
    ControlCallbacks[IDC_ENABLEASYNCCOMPUTE] = { &UHSettingDialog::ControlAsyncCompute };
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
        GLatestControl = wParam;
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            GSettingWindow = nullptr;
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void UHSettingDialog::ShowDialog()
{
    if (GSettingWindow == nullptr)
    {
        GSettingWindow = CreateDialog(Instance, MAKEINTRESOURCE(IDD_SETTING), Window, (DLGPROC)SettingProc);

        // sync ini settings to the window controls
        const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
        const UHEngineSettings& EngineSettings = Config->EngineSetting();
        const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

        // presentation
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_VSYNC, PresentSettings.bVsync);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_FULLSCREEN, PresentSettings.bFullScreen);

        // engine
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_CAMERASPEED, std::to_wstring((int)EngineSettings.DefaultCameraMoveSpeed));
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_MOUSEROTATESPEED, std::to_wstring((int)EngineSettings.MouseRotationSpeed));
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_FORWARDKEY, EngineSettings.ForwardKey);
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_BACKKEY, EngineSettings.BackKey);
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_LEFTKEY, EngineSettings.LeftKey);
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_RIGHTKEY, EngineSettings.RightKey);
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_DOWNKEY, EngineSettings.DownKey);
        UHEditorUtil::SetEditControlChar(GSettingWindow, IDC_UPKEY, EngineSettings.UpKey);
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_FPSLIMIT, std::to_wstring((int)EngineSettings.FPSLimit));
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_MESHBUFFERMEMORYBUDGET, std::to_wstring((int)EngineSettings.MeshBufferMemoryBudgetMB));
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_IMAGEMEMORYBUDGET, std::to_wstring((int)EngineSettings.ImageMemoryBudgetMB));

        // rendering
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_RENDERWIDTH, std::to_wstring(RenderingSettings.RenderWidth));
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_RENDERHEIGHT, std::to_wstring(RenderingSettings.RenderHeight));
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLETAA, RenderingSettings.bTemporalAA);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLERAYTRACING, RenderingSettings.bEnableRayTracing);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLEGPULABELING, RenderingSettings.bEnableGPULabeling);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLELAYERVALIDATION, RenderingSettings.bEnableLayerValidation);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLEGPUTIMING, RenderingSettings.bEnableGPUTiming);
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLEPARALLELSUBMISSION, RenderingSettings.bParallelSubmission);
        UHEditorUtil::SetEditControl(GSettingWindow, IDC_PARALLELTHREADS, std::to_wstring(RenderingSettings.ParallelThreads));
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLEDEPTHPREPASS, RenderingSettings.bEnableDepthPrePass);

        std::vector<std::wstring> ShadowQualities = { L"Full", L"Half", L"Quarter" };
        UHEditorUtil::InitComboBox(GSettingWindow, IDC_RTSHADOWQUALITY, ShadowQualities[RenderingSettings.RTDirectionalShadowQuality], ShadowQualities);

        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLEASYNCCOMPUTE, RenderingSettings.bEnableAsyncCompute);

        ShowWindow(GSettingWindow, SW_SHOW);
    }
}

void UHSettingDialog::Update()
{
    if (!GSettingWindow)
    {
        return;
    }

    if (GLatestControl != 0)
    {
        if (ControlCallbacks.find(LOWORD(GLatestControl)) != ControlCallbacks.end())
        {
            if (HIWORD(GLatestControl) == EN_CHANGE || HIWORD(GLatestControl) == BN_CLICKED
                || HIWORD(GLatestControl) == CBN_SELCHANGE)
            {
                (this->*ControlCallbacks[LOWORD(GLatestControl)])();
            }
        }

        GLatestControl = 0;
    }

    // sync settings from input event
    const UHPresentationSettings& PresentSettings = Config->PresentationSetting();
    const UHEngineSettings& EngineSettings = Config->EngineSetting();
    const UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

    // full screen toggling
    if (Input->IsKeyHold(VK_MENU) && Input->IsKeyUp(VK_RETURN))
    {
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_FULLSCREEN, !PresentSettings.bFullScreen);
    }

    // TAA toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('t'))
    {
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_ENABLETAA, !RenderingSettings.bTemporalAA);
    }

    // vsync toggling
    if (Input->IsKeyHold(VK_CONTROL) && Input->IsKeyUp('v'))
    {
        UHEditorUtil::SetCheckedBox(GSettingWindow, IDC_VSYNC, !PresentSettings.bVsync);
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
    EngineSettings.DefaultCameraMoveSpeed = UHEditorUtil::GetEditControl<float>(GSettingWindow, IDC_CAMERASPEED);
}

void UHSettingDialog::ControlMouseRotateSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MouseRotationSpeed = UHEditorUtil::GetEditControl<float>(GSettingWindow, IDC_MOUSEROTATESPEED);
}

void UHSettingDialog::ControlForwardKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ForwardKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_FORWARDKEY);
}

void UHSettingDialog::ControlBackKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.BackKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_BACKKEY);
}

void UHSettingDialog::ControlLeftKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.LeftKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_LEFTKEY);
}

void UHSettingDialog::ControlRightKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.RightKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_RIGHTKEY);
}

void UHSettingDialog::ControlDownKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DownKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_DOWNKEY);
}

void UHSettingDialog::ControlUpKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.UpKey = UHEditorUtil::GetEditControlChar(GSettingWindow, IDC_UPKEY);
}

void UHSettingDialog::ControlFPSLimit()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.FPSLimit = UHEditorUtil::GetEditControl<float>(GSettingWindow, IDC_FPSLIMIT);
}

void UHSettingDialog::ControlBufferMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MeshBufferMemoryBudgetMB = UHEditorUtil::GetEditControl<float>(GSettingWindow, IDC_MESHBUFFERMEMORYBUDGET);
}

void UHSettingDialog::ControlImageMemoryBudget()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ImageMemoryBudgetMB = UHEditorUtil::GetEditControl<float>(GSettingWindow, IDC_IMAGEMEMORYBUDGET);
}

void UHSettingDialog::ControlResolution()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    int32_t Width = UHEditorUtil::GetEditControl<int32_t>(GSettingWindow, IDC_RENDERWIDTH);
    int32_t Height = UHEditorUtil::GetEditControl<int32_t>(GSettingWindow, IDC_RENDERHEIGHT);

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
    RenderingSettings.ParallelThreads = UHEditorUtil::GetEditControl<int32_t>(GSettingWindow, IDC_PARALLELTHREADS);
}

void UHSettingDialog::ControlShadowQuality()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.RTDirectionalShadowQuality = UHEditorUtil::GetComboBoxSelectedIndex(GSettingWindow, IDC_RTSHADOWQUALITY);
    DeferredRenderer->ResizeRTBuffers();
}

void UHSettingDialog::ControlAsyncCompute()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableAsyncCompute = !RenderingSettings.bEnableAsyncCompute;
}

#endif