#include "Editor.h"

#if WITH_EDITOR

#include "../../resource.h"
#include "Runtime/Engine/Engine.h"
#include "Runtime/Engine/Config.h"
#include "Runtime/Renderer/DeferredShadingRenderer.h"
#include "../../Runtime/Engine/Input.h"
#include "Profiler.h"
#include "../Classes/EditorUtils.h"
#include "Runtime/Classes/AssetPath.h"
#include "../Dialog/StatusDialog.h"

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
    WorldDialog = MakeUnique<UHWorldDialog>(HWnd, DeferredRenderer);
    TextureDialog = MakeUnique<UHTextureDialog>(AssetManager, InGfx, InRenderer);
    MaterialDialog = MakeUnique<UHMaterialDialog>(HInstance, HWnd, AssetManager, InRenderer);
    CubemapDialog = MakeUnique<UHCubemapDialog>(AssetManager, InGfx, InRenderer);
    InfoDialog = MakeUnique<UHInfoDialog>(HWnd, WorldDialog.get());
    MeshDialog = MakeUnique<UHMeshDialog>(AssetManager, InGfx, InRenderer, InInput);

    // always showing world / info dialog after initialization
    WorldDialog->ShowDialog();
    InfoDialog->ShowDialog();
}

void UHEditor::OnEditorUpdate()
{
    bool bIsDialogActive = false;
    Input->SetInputEnabled(true);
    ProfileTimer.Tick();
    SettingDialog->Update(bIsDialogActive);
    ProfileDialog->SyncProfileStatistics(Profile, &ProfileTimer, Config);
    TextureDialog->Update(bIsDialogActive);
    MaterialDialog->Update(bIsDialogActive);
    CubemapDialog->Update(bIsDialogActive);
    MeshDialog->Update(bIsDialogActive);

    if (!Config->PresentationSetting().bFullScreen)
    {
        WorldDialog->Update(bIsDialogActive);
        InfoDialog->Update(bIsDialogActive);
    }

    Input->SetInputEnabled(!bIsDialogActive);
    UHGameTimerScope::ClearRegisteredGameTime();
}

void UHEditor::OnEditorMove()
{
    WorldDialog->ResetDialogWindow();
    InfoDialog->ResetDialogWindow();
}

void UHEditor::OnEditorResize()
{
    WorldDialog->ResetDialogWindow();
    InfoDialog->ResetDialogWindow();
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
    case ID_WINDOW_CUBEMAPEDITOR:
        CubemapDialog->ShowDialog();
        break;
    case ID_WINDOW_MESHEDITOR:
        MeshDialog->ShowDialog();
        break;
    case ID_FILE_SAVESCENE:
        OnSaveScene();
        break;
    case ID_FILE_LOADSCENE:
        OnLoadScene();
        break;
    default:
        break;
    } 
}

void UHEditor::EvaluateEditorDelta(uint32_t& OutW, uint32_t& OutH)
{
    OutW += static_cast<uint32_t>(WorldDialog->GetWindowSize().x);
    OutH += static_cast<uint32_t>(InfoDialog->GetWindowSize().y);
}

void UHEditor::RefreshWorldDialog()
{
    WorldDialog->ShowDialog();
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
        , ID_VIEWMODE_RTREFLECTION};

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

void UHEditor::OnSaveScene()
{
    if (!std::filesystem::exists(GSceneAssetPath))
    {
        std::filesystem::create_directories(GSceneAssetPath);
    }

    std::wstring Ext = L"*" + UHUtilities::ToStringW(GSceneAssetExtension);
    COMDLG_FILTERSPEC Filter{L"Unheard Engine Scene File", Ext.c_str() };

    std::filesystem::path SceneAssetPath = GSceneAssetPath;
    SceneAssetPath = std::filesystem::absolute(SceneAssetPath);
    std::filesystem::path OutPath = UHEditorUtil::FileSelectSavePath(Filter, SceneAssetPath.wstring());
    if (OutPath.string().length() == 0)
    {
        return;
    }

    if (!OutPath.has_extension())
    {
        OutPath += GSceneAssetExtension;
    }

    bool bIsValidOutputFolder = false;
    bIsValidOutputFolder |= UHUtilities::StringFind(OutPath.string() + "\\", SceneAssetPath.string());
    bIsValidOutputFolder |= UHUtilities::StringFind(SceneAssetPath.string(), OutPath.string() + "\\");

    std::filesystem::path Temp = OutPath;
    if (!std::filesystem::exists(Temp.remove_filename()) || !bIsValidOutputFolder)
    {
        MessageBoxA(nullptr, "Invalid output folder or it's not under the engine path Assets/Scenes!", "Scene save", MB_OK);
        return;
    }

    Engine->OnSaveScene(OutPath);
    MessageBoxA(nullptr, "Scene saved!", "UHE", MB_OK);
}

void UHEditor::OnLoadScene()
{
    if (!std::filesystem::exists(GSceneAssetPath))
    {
        std::filesystem::create_directories(GSceneAssetPath);
    }

    std::wstring Ext = L"*" + UHUtilities::ToStringW(GSceneAssetExtension);
    COMDLG_FILTERSPEC Filter{ L"Unheard Engine Scene File", Ext.c_str() };

    std::filesystem::path SceneAssetPath = GSceneAssetPath;
    SceneAssetPath = std::filesystem::absolute(SceneAssetPath);
    std::filesystem::path InputPath = UHEditorUtil::FileSelectInput(Filter, SceneAssetPath.wstring());
    if (InputPath.string().length() == 0)
    {
        return;
    }

    if (!std::filesystem::exists(InputPath))
    {
        MessageBoxA(nullptr, "Invalid scene file!", "Scene load", MB_OK);
        return;
    }

    {
        UHStatusDialogScope Status("Loading...");
        Engine->OnLoadScene(InputPath);
    }
}

#endif