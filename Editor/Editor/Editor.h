#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <unordered_map>
#include <memory>
#include "../../Runtime/Engine/GameTimer.h"
#include "../Dialog/ProfileDialog.h"
#include "../Dialog/SettingDialog.h"
#include "../Dialog/MaterialDialog.h"

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
	~UHEditor();

	void OnEditorUpdate();
	void OnMenuSelection(int32_t WmId);

private:
	void SelectDebugViewModeMenu(int32_t WmId);

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
	UHProfileDialog ProfileDialog;
	UHSettingDialog SettingDialog;
	std::unique_ptr<UHMaterialDialog> MaterialDialog;
};

#endif