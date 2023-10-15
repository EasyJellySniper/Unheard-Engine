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

#if WITH_EDITOR
#include "../../Editor/Editor/Editor.h"
#endif
#include "../../Editor/Editor/Profiler.h"

enum UHEngineResizeReason
{
	NotResizing = 0,
	FromWndMessage,
	ToggleFullScreen,
	ToggleVsync,
	ToggleHDR,
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

#if WITH_EDITOR
	UHEditor* GetEditor() const;
	UHGraphic* GetGfx() const;

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
	UniquePtr<UHGraphic> UHEGraphic;

	// input class
	UniquePtr<UHRawInput> UHERawInput;

	// game timer class
	UniquePtr<UHGameTimer> UHEGameTimer;

	// asset class
	UniquePtr<UHAssetManager> UHEAsset;

	// config class
	UniquePtr<UHConfigManager> UHEConfig;

	// renderer class
	UniquePtr<UHDeferredShadingRenderer> UHERenderer;

#if WITH_EDITOR
	// editor class
	UniquePtr<UHEditor> UHEEditor;
	std::wstring WindowCaption;
#endif

	// profiler class
	UHProfiler UHEProfiler;
	UHProfiler MainThreadProfile;

	// scene define
	// @TODO: better scene management
	UHScene DefaultScene;

	// a flag which tells if the engine is initialized
	bool bIsInitialized;

	// resize reason
	UHEngineResizeReason EngineResizeReason;

	int64_t FrameBeginTime;
};

