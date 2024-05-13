#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include <unordered_map>
#include <memory>
#include "Runtime/Engine/GameTimer.h"
#include "../Dialog/ProfileDialog.h"
#include "../Dialog/SettingDialog.h"
#include "../Dialog/MaterialDialog.h"
#include "../Dialog/TextureDialog.h"
#include "../Dialog/WorldDialog.h"
#include "../Dialog/CubemapDialog.h"
#include "../Dialog/InfoDialog.h"
#include "../Dialog/MeshDialog.h"

class UHEngine;
class UHGraphic;
class UHConfigManager;
class UHDeferredShadingRenderer;
class UHRawInput;
class UHProfiler;
class UHAssetManager;

class UHEditor
{
public:
	UHEditor(HINSTANCE InInstance, HWND InHwnd, UHEngine* InEngine, UHConfigManager* InConfig, UHDeferredShadingRenderer* InRenderer
		, UHRawInput* InInput, UHProfiler* InProfile, UHAssetManager* InAssetManager, UHGraphic* InGfx);

	void OnEditorUpdate();
	void OnEditorMove();
	void OnEditorResize();
	void OnMenuSelection(int32_t WmId);
	void EvaluateEditorDelta(uint32_t& OutW, uint32_t& OutH);
	void RefreshWorldDialog();

private:
	void SelectDebugViewModeMenu(int32_t WmId);
	void OnSaveScene();
	void OnLoadScene();

	HINSTANCE HInstance;
	HWND HWnd;
	UHEngine* Engine;
	UHConfigManager* Config;
	UHDeferredShadingRenderer* DeferredRenderer;
	UHRawInput* Input;
	UHProfiler* Profile;
	UHAssetManager* AssetManager;
	UHGraphic* Gfx;
	UHGameTimer ProfileTimer;
	int32_t ViewModeMenuItem;

	// custom dialogs
	UniquePtr<UHProfileDialog> ProfileDialog;
	UniquePtr<UHSettingDialog> SettingDialog;
	UniquePtr<UHWorldDialog> WorldDialog;
	UniquePtr<UHTextureDialog> TextureDialog;
	UniquePtr<UHMaterialDialog> MaterialDialog;
	UniquePtr<UHCubemapDialog> CubemapDialog;
	UniquePtr<UHInfoDialog> InfoDialog;
	UniquePtr<UHMeshDialog> MeshDialog;
};

#endif