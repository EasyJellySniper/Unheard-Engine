#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <unordered_map>

class UHEngine;
class UHConfigManager;
class UHDeferredShadingRenderer;
class UHRawInput;

class UHEditor
{
public:
	UHEditor(HINSTANCE InInstance, HWND InHwnd, UHEngine* InEngine, UHConfigManager* InConfig, UHDeferredShadingRenderer* InRenderer
		, UHRawInput* InInput);
	void OnEditorUpdate();
	void OnMenuSelection(int32_t WmId);

private:
	void SelectDebugViewModeMenu(int32_t WmId);
	void ProcessSettingWindow(int32_t WmId);

	// control functions
	void ControlVsync();
	void ControlFullScreen();
	void ControlCameraSpeed();
	void ControlMouseRotateSpeed();
	void ControlForwardKey();
	void ControlBackKey();
	void ControlLeftKey();
	void ControlRightKey();
	void ControlDownKey();
	void ControlUpKey();
	void ControlResolution();
	void ControlTAA();
	void ControlRayTracing();
	void ControlGPULabeling();
	void ControlShadowQuality();

	HINSTANCE HInstance;
	HWND HWnd;
	UHEngine* Engine;
	UHConfigManager* Config;
	UHDeferredShadingRenderer* DeferredRenderer;
	UHRawInput* Input;
	int32_t ViewModeMenuItem;

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHEditor::*)()> ControlCallbacks;
};

#endif