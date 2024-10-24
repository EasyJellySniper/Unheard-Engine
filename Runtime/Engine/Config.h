#pragma once
#include "../Classes/Settings.h"
#include "../../framework.h"

class UHConfigManager
{
public:
	UHConfigManager();

	// load config
	void LoadConfig();

	// save config
	void SaveConfig(HWND InWindow);

	// apply presentation settings
	void ApplyPresentationSettings(HWND InWindow);
	void UpdateWindowSize(HWND InWindow);

	// apply window style
	void ApplyWindowStyle(HINSTANCE InInstance, HWND InWindow);

	// toggle settings
	void ToggleTAA();
	void ToggleVsync();

	// get settings
	UHPresentationSettings& PresentationSetting();
	UHEngineSettings& EngineSetting();
	UHRenderingSettings& RenderingSetting();

	// toggle full screen flag in presentation settings
	void ToggleFullScreen();

private:
	// presentation settings
	UHPresentationSettings PresentationSettings;

	// engine settings
	UHEngineSettings EngineSettings;

	// rendering settings
	UHRenderingSettings RenderingSettings;
};