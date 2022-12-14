#pragma once
#include "../../framework.h"
#include "../Classes/Settings.h"
#include "Config.h"
#include "Graphic.h"
#include "Input.h"
#include "GameTimer.h"
#include "Asset.h"
#include "../Renderer/DeferredShadingRenderer.h"
#include "../Classes/Scene.h"
#include <memory>
#include <string>

#if WITH_DEBUG
#include "../../Editor/Profiler.h"
#include "../../Editor/EditorUI/Editor.h"
#endif

enum UHEngineResizeReason
{
	NotResizing = 0,
	FromWndMessage,
	ToggleFullScreen,
	ToggleVsync,
	NewResolution
};

// Unheard engine class
class UHEngine
{
public:
	UHEngine();

	// init engine
	bool InitEngine(HINSTANCE Instance, HWND EngineWindow);

	// Release engine
	void ReleaseEngine();

	// is engine initialized?
	bool IsEngineInitialized();

	// load config
	void LoadConfig();

	// save config
	void SaveConfig();

	// update function
	void Update();

	// render loop function
	void RenderLoop();

	// toggle full screen
	void ToggleFullScreen();

	// set is need resize
	void SetResizeReason(UHEngineResizeReason InFlag);

	// get raw input
	UHRawInput* GetRawInput() const;

	// FPS limiter function
	void BeginFPSLimiter();
	void EndFPSLimiter();

#if WITH_DEBUG
	UHEditor* GetEditor() const;
	void BeginProfile();
	void EndProfile();
#endif

private:
	// engine resize
	void ResizeEngine();

	// cache of main window
	HWND UHEngineWindow;

	// cache of HInstance
	HINSTANCE UHWindowInstance;

	// graphic class
	std::unique_ptr<UHGraphic> UHEGraphic;

	// input class
	std::unique_ptr<UHRawInput> UHERawInput;

	// game timer class
	std::unique_ptr<UHGameTimer> UHEGameTimer;

	// asset class
	std::unique_ptr<UHAssetManager> UHEAsset;

	// config class
	std::unique_ptr<UHConfigManager> UHEConfig;

	// renderer class
	std::unique_ptr<UHDeferredShadingRenderer> UHERenderer;

#if WITH_DEBUG
	// editor class
	std::unique_ptr<UHEditor> UHEEditor;

	// profiler class
	UHProfiler UHEProfiler;
	UHProfiler MainThreadProfile;
	std::wstring WindowCaption;
#endif

	// scene define
	// @TODO: better scene management
	UHScene DefaultScene;

	// a flag which tells if the engine is initialized
	bool bIsInitialized;

	// resize reason
	UHEngineResizeReason EngineResizeReason;

	int64_t FrameBeginTime;
};

