#include "Editor.h"

#if WITH_DEBUG

#include "../../resource.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Engine/Input.h"
#include "../Editor/Profiler.h"
#include "EditorUtils.h"

UHEditor::UHEditor(HINSTANCE InInstance, HWND InHwnd, UHEngine* InEngine, UHConfigManager* InConfig, UHDeferredShadingRenderer* InRenderer
    , UHRawInput* InInput, UHProfiler* InProfile)
	: HInstance(InInstance)
    , HWnd(InHwnd)
    , Engine(InEngine)
    , Config(InConfig)
	, DeferredRenderer(InRenderer)
    , Input(InInput)
    , Profile(InProfile)
    , ViewModeMenuItem(ID_VIEWMODE_FULLLIT)
{
    ProfileTimer.Reset();
    SettingDialog = UHSettingDialog(HInstance, HWnd, Config, Engine, DeferredRenderer, Input);
    ProfileDialog = UHProfileDialog(HInstance, HWnd);
}

void UHEditor::OnEditorUpdate()
{
    ProfileTimer.Tick();
    SettingDialog.Update();
    ProfileDialog.SyncProfileStatistics(Profile, &ProfileTimer, Config);
}

void UHEditor::OnMenuSelection(int32_t WmId)
{
    SelectDebugViewModeMenu(WmId);

    switch (WmId)
    {
    case ID_WINDOW_SETTINGS:
        SettingDialog.ShowDialog();
        break;
    case ID_WINDOW_PROFILES:
        ProfileDialog.ShowDialog();
        break;
    default:
        break;
    } 
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

#endif