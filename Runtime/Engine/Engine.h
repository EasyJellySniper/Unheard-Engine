#pragma once
#include "../../framework.h"
#include "../Classes/Settings.h"
#include "Config.h"
#include "Graphic.h"
#include "Runtime/Platform/PlatformInput.h"
#include "GameTimer.h"
#include "Asset.h"
#include "../Renderer/DeferredShadingRenderer.h"
#include "../Classes/Scene.h"
#include "../Classes/Thread.h"
#include <memory>
#include <string>

#if WITH_EDITOR
#include "../../Editor/Editor/Editor.h"
#include "../../Editor/Editor/Profiler.h"
#endif

enum class UHEngineResizeReason : uint32_t
{
	NotResizing = 0,
	FromClientCallback,
	ToggleFullScreen,
	ToggleVsync,
	ToggleHDR,
	NewResolution
};

class UHClient;

// Unheard engine class
class UHEngine
{
public:
	UHEngine();

	// init engine
	bool InitEngine(UHClient* InClient);

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

	// UH interface getters
	UHPlatformInput* GetRawInput() const;
	UHGraphic* GetGfx() const;
	UHGameTimer* GetGameTimer() const;
	UHAssetManager* GetAssetManager() const;
	UHConfigManager* GetConfigManager() const;
	UHDeferredShadingRenderer* GetSceneRenderer() const;
	UHScene* GetScene() const;

	// FPS limiter function
	void BeginFPSLimiter();
	void EndFPSLimiter();
	void DisplayFPS();

	void ResetScene();
	void OnSaveScene(std::filesystem::path OutputPath);
	void OnLoadScene(std::filesystem::path InputPath);

#if WITH_EDITOR
	UHEditor* GetEditor() const;

	void BeginProfile();
	void EndProfile();
#endif

private:
	// engine resize
	void ResizeEngine();
	void DisplayFPSTitle(float InDurationMS);

	// cache the client
	UHClient* UHEClient;

	// graphic class
	UniquePtr<UHGraphic> UHEGraphic;

	// input class
	UniquePtr<UHPlatformInput> UHERawInput;

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
	// profiler class
	UHProfiler UHEProfiler;
	UHProfiler EngineUpdateProfile;
#endif
	std::string WindowCaption;

	// scene define
	UniquePtr<UHScene> CurrentScene;

	// a flag which tells if the engine is initialized
	bool bIsInitialized;

	// resize reason
	UHEngineResizeReason EngineResizeReason;

	UHClock::time_point FrameBeginTime;
	UHClock::time_point FrameEndTime;
	float DisplayFrequency;
	float AverageFrameTimeMS;
	uint32_t FrameCount;
};

