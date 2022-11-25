#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>

class UHConfigManager;
class UHEngine;
class UHDeferredShadingRenderer;
class UHRawInput;

class UHSettingDialog : public UHDialog
{
public:
	UHSettingDialog();
	UHSettingDialog(HINSTANCE InInstance, HWND InWindow, UHConfigManager* InConfig, UHEngine* InEngine
		, UHDeferredShadingRenderer* InRenderer, UHRawInput* InRawInput);
	virtual void ShowDialog() override;
	void Update();

private:

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
	void ControlLayerValidation();
	void ControlGPUTiming();
	void ControlShadowQuality();

	UHConfigManager* Config;
	UHEngine* Engine;
	UHDeferredShadingRenderer* DeferredRenderer;
	UHRawInput* Input;

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHSettingDialog::*)()> ControlCallbacks;
};

#endif