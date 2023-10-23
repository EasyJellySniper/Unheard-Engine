#include "Editor.h"

#if WITH_EDITOR

#include "../../resource.h"
#include "../../Runtime/Engine/Engine.h"
#include "../../Runtime/Engine/Config.h"
#include "../../Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Engine/Input.h"
#include "Profiler.h"
#include "../Classes/EditorUtils.h"

UHEditor::UHEditor(HINSTANCE InInstance, HWND InHwnd, UHEngine* InEngine, UHConfigManager* InConfig, UHDeferredShadingRenderer* InRenderer
    , UHRawInput* InInput, UHProfiler* InProfile, UHAssetManager* InAssetManager, UHGraphic* InGfx)
	: HInstance(InInstance)
    , HWnd(InHwnd)
    , Engine(InEngine)
    , Config(InConfig)
	, DeferredRenderer(InRenderer)
    , Input(InInput)
    , Profile(InProfile)
    , AssetManager(InAssetManager)
    , Gfx(InGfx)
    , ViewModeMenuItem(ID_VIEWMODE_FULLLIT)
{
    ProfileTimer.Reset();
    SettingDialog = MakeUnique<UHSettingDialog>(Config, Engine, DeferredRenderer);
    ProfileDialog = MakeUnique<UHProfileDialog>();
    DetailDialog = MakeUnique<UHDetailDialog>(HWnd);
    WorldDialog = MakeUnique<UHWorldDialog>(HWnd, DeferredRenderer, DetailDialog.get());
    TextureDialog = MakeUnique<UHTextureDialog>(AssetManager, InGfx, InRenderer);
    MaterialDialog = MakeUnique<UHMaterialDialog>(HInstance, HWnd, AssetManager, InRenderer);
}

UHEditor::~UHEditor()
{
    MaterialDialog.reset();
    TextureDialog.reset();
}

void UHEditor::OnEditorUpdate()
{
    ProfileTimer.Tick();
    SettingDialog->Update();
    ProfileDialog->SyncProfileStatistics(Profile, &ProfileTimer, Config);
    TextureDialog->Update();
    MaterialDialog->Update();
    DetailDialog->Update();
    WorldDialog->Update();
}

void UHEditor::OnEditorMove()
{
    WorldDialog->ResetDialogWindow();
    DetailDialog->ResetDialogWindow();
}

void UHEditor::OnEditorResize()
{
    WorldDialog->ResetDialogWindow();
    DetailDialog->ResetDialogWindow();
}

void UHEditor::OnMenuSelection(int32_t WmId)
{
    SelectDebugViewModeMenu(WmId);

    switch (WmId)
    {
    case ID_WINDOW_SETTINGS:
        SettingDialog->ShowDialog();
        break;
    case ID_WINDOW_PROFILES:
        ProfileDialog->ShowDialog();
        break;
    case ID_WINDOW_MATERIAL:
        MaterialDialog->ShowDialog();
        break;
    case ID_WINDOW_TEXTURE:
        TextureDialog->ShowDialog();
        break;
    case ID_WINDOW_WORLDEDITOR:
        WorldDialog->ShowDialog();
        break;
    default:
        break;
    } 
}

void UHEditor::SelectDebugViewModeMenu(int32_t WmId)
{
    const std::vector<int32_t> ViewModeMenuIDs = { ID_VIEWMODE_FULLLIT
        , ID_VIEWMODE_DIFFUSE
        , ID_VIEWMODE_NORMAL
        , ID_VIEWMODE_PBR
        , ID_VIEWMODE_DEPTH
        , ID_VIEWMODE_MOTION
        , ID_VIEWMODE_VERTEXNORMALMIP
        , ID_VIEWMODE_RTSHADOW
        , ID_VIEWMODE_RTSHADOW_TRANSLUCENT};

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