#include "Editor.h"

#if WITH_DEBUG

#include "../../resource.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Engine/Input.h"
#include "EditorUtils.h"
#include <vector>

// global variables, some variable needs to be accesses in callback function
HWND SettingWindow = nullptr;
WPARAM LatestControl = 0;

UHEditor::UHEditor(HINSTANCE InInstance, HWND InHwnd, UHEngine* InEngine, UHConfigManager* InConfig, UHDeferredShadingRenderer* InRenderer
    , UHRawInput* InInput)
	: HInstance(InInstance)
    , HWnd(InHwnd)
    , Engine(InEngine)
    , Config(InConfig)
	, DeferredRenderer(InRenderer)
    , Input(InInput)
    , ViewModeMenuItem(ID_VIEWMODE_FULLLIT)
{
    // add callbacks
    ControlCallbacks[IDC_VSYNC] = { &UHEditor::ControlVsync };
    ControlCallbacks[IDC_FULLSCREEN] = { &UHEditor::ControlFullScreen };
    ControlCallbacks[IDC_CAMERASPEED] = { &UHEditor::ControlCameraSpeed };
    ControlCallbacks[IDC_MOUSEROTATESPEED] = { &UHEditor::ControlMouseRotateSpeed };
    ControlCallbacks[IDC_FORWARDKEY] = { &UHEditor::ControlForwardKey };
    ControlCallbacks[IDC_BACKKEY] = { &UHEditor::ControlBackKey };
    ControlCallbacks[IDC_LEFTKEY] = { &UHEditor::ControlLeftKey };
    ControlCallbacks[IDC_RIGHTKEY] = { &UHEditor::ControlRightKey };
    ControlCallbacks[IDC_DOWNKEY] = { &UHEditor::ControlDownKey };
    ControlCallbacks[IDC_UPKEY] = { &UHEditor::ControlUpKey };
    ControlCallbacks[IDC_APPLYRESOLUTION] = { &UHEditor::ControlResolution };
    ControlCallbacks[IDC_ENABLETAA] = { &UHEditor::ControlTAA };
    ControlCallbacks[IDC_ENABLERAYTRACING] = { &UHEditor::ControlRayTracing };
    ControlCallbacks[IDC_ENABLEGPULABELING] = { &UHEditor::ControlGPULabeling };
    ControlCallbacks[IDC_RTSHADOWQUALITY] = { &UHEditor::ControlShadowQuality };
}

void UHEditor::OnEditorUpdate()
{
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
    UHPresentationSettings& PresentSettings = Config->PresentationSetting();
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

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

void UHEditor::OnMenuSelection(int32_t WmId)
{
    SelectDebugViewModeMenu(WmId);
    ProcessSettingWindow(WmId);
}

void UHEditor::SelectDebugViewModeMenu(int32_t WmId)
{
    std::vector<int32_t> ViewModeMenuIDs = { ID_VIEWMODE_FULLLIT
        , ID_VIEWMODE_DIFFUSE
        , ID_VIEWMODE_NORMAL
        , ID_VIEWMODE_PBR
        , ID_VIEWMODE_DEPTH
        , ID_VIEWMODE_MOTION
        , ID_VIEWMODE_VERTEXNORMALMIP
        , ID_VIEWMODE_RTSHADOW };

    for (size_t Idx = 0; Idx < ViewModeMenuIDs.size(); Idx++)
    {
        if (WmId == ViewModeMenuIDs[Idx])
        {
            // update debug view index and menu checked state
            DeferredRenderer->SetDebugViewIndex(static_cast<int32_t>(Idx));

            UHEditorUtil::SetMenuItemChecked(HWnd, ViewModeMenuIDs[Idx], MFS_CHECKED);
            UHEditorUtil::SetMenuItemChecked(HWnd, ViewModeMenuItem, MFS_UNCHECKED);
            ViewModeMenuItem = ViewModeMenuIDs[Idx];

            break;
        }
    }
}

// Message handler for setting window
INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
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

void UHEditor::ProcessSettingWindow(int32_t WmId)
{
    if (WmId != ID_WINDOW_SETTINGS)
    {
        return;
    }

    if (SettingWindow == nullptr)
    {
        SettingWindow = CreateDialog(HInstance, MAKEINTRESOURCE(IDD_SETTING), HWnd, (DLGPROC)Settings);

        // sync ini settings to the window controls
        UHPresentationSettings& PresentSettings = Config->PresentationSetting();
        UHEngineSettings& EngineSettings = Config->EngineSetting();
        UHRenderingSettings& RenderingSettings = Config->RenderingSetting();

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

        // rendering
        UHEditorUtil::SetEditControl(SettingWindow, IDC_RENDERWIDTH, std::to_wstring(RenderingSettings.RenderWidth));
        UHEditorUtil::SetEditControl(SettingWindow, IDC_RENDERHEIGHT, std::to_wstring(RenderingSettings.RenderHeight));
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLETAA, RenderingSettings.bTemporalAA);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLERAYTRACING, RenderingSettings.bEnableRayTracing);
        UHEditorUtil::SetCheckedBox(SettingWindow, IDC_ENABLEGPULABELING, RenderingSettings.bEnableGPULabeling);

        std::vector<std::wstring> ShadowQualities = { L"Full", L"Half", L"Quarter" };
        UHEditorUtil::InitComboBox(SettingWindow, IDC_RTSHADOWQUALITY, ShadowQualities[RenderingSettings.RTDirectionalShadowQuality], ShadowQualities);

        ShowWindow(SettingWindow, SW_SHOW);
    }
}

void UHEditor::ControlVsync()
{
    Config->ToggleVsync();
    Engine->SetIsNeedResize(true);
}

void UHEditor::ControlFullScreen()
{
    Engine->ToggleFullScreen();
}

void UHEditor::ControlCameraSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DefaultCameraMoveSpeed = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_CAMERASPEED);
}

void UHEditor::ControlMouseRotateSpeed()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.MouseRotationSpeed = UHEditorUtil::GetEditControl<float>(SettingWindow, IDC_MOUSEROTATESPEED);
}

void UHEditor::ControlForwardKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.ForwardKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_FORWARDKEY);
}

void UHEditor::ControlBackKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.BackKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_BACKKEY);
}

void UHEditor::ControlLeftKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.LeftKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_LEFTKEY);
}

void UHEditor::ControlRightKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.RightKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_RIGHTKEY);
}

void UHEditor::ControlDownKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.DownKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_DOWNKEY);
}

void UHEditor::ControlUpKey()
{
    UHEngineSettings& EngineSettings = Config->EngineSetting();
    EngineSettings.UpKey = UHEditorUtil::GetEditControlChar(SettingWindow, IDC_UPKEY);
}

void UHEditor::ControlResolution()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    int32_t Width = UHEditorUtil::GetEditControl<int32_t>(SettingWindow, IDC_RENDERWIDTH);
    int32_t Height = UHEditorUtil::GetEditControl<int32_t>(SettingWindow, IDC_RENDERHEIGHT);

    if (Width > 0 && Height > 0)
    {
        RenderingSettings.RenderWidth = Width;
        RenderingSettings.RenderHeight = Height;
        Engine->SetIsNeedResize(true);
    }
}

void UHEditor::ControlTAA()
{
    Config->ToggleTAA();
}

void UHEditor::ControlRayTracing()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableRayTracing = !RenderingSettings.bEnableRayTracing;
}

void UHEditor::ControlGPULabeling()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.bEnableGPULabeling = !RenderingSettings.bEnableGPULabeling;
}

void UHEditor::ControlShadowQuality()
{
    UHRenderingSettings& RenderingSettings = Config->RenderingSetting();
    RenderingSettings.RTDirectionalShadowQuality = UHEditorUtil::GetComboBoxSelectedIndex(SettingWindow, IDC_RTSHADOWQUALITY);
    DeferredRenderer->ResizeRTBuffers();
}

#endif