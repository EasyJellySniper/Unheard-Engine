#include "Engine.h"
#include <fstream>
#include <string>
#include "../CoreGlobals.h"
#include "../Components/GameScript.h"

#if WITH_DEBUG
#include <sstream>
#include <iomanip>
#endif

UHEngine::UHEngine()
	: UHEngineWindow(nullptr)
	, UHWindowInstance(nullptr)
	, bIsInitialized(false)
	, EngineResizeReason(UHEngineResizeReason::NotResizing)
	, FrameBeginTime(0)
#if WITH_DEBUG
	, UHEEditor(nullptr)
	, UHEProfiler(nullptr)
	, WindowCaption(ENGINE_NAMEW)
#endif
{
	// config manager needs to be initialze as early as possible
	UHEConfig = std::make_unique<UHConfigManager>();
}

// function to load settings from config file
void UHEngine::LoadConfig()
{
	UHEConfig->LoadConfig();
}

// function to save settings to config file
void UHEngine::SaveConfig()
{
	UHEConfig->SaveConfig(UHEngineWindow);
}

bool UHEngine::InitEngine(HINSTANCE Instance, HWND EngineWindow)
{
	// init asset manager
	UHEAsset = std::make_unique<UHAssetManager>();

	// init graphic 
	const UHPresentationSettings PresentationSettings = UHEConfig->PresentationSetting();
	UHEngineWindow = EngineWindow;
	UHWindowInstance = Instance;

	UHEGraphic = std::make_unique<UHGraphic>(UHEAsset.get(), UHEConfig.get());
	if (!UHEGraphic->InitGraphics(UHEngineWindow))
	{
		UHE_LOG(L"Can't initialize graphic class!\n");
		return false;
	}

	// import assets, texture needs graphic interface
	UHEAsset->ImportTextures(UHEGraphic.get());
	UHEAsset->ImportMeshes();
	UHEAsset->ImportMaterials(UHEGraphic.get());

	// init input 
	UHERawInput = std::make_unique<UHRawInput>();
	if (!UHERawInput->InitRawInput())
	{
		// print a log to remind users that inputs aren't available, and it's okay to proceed
		UHE_LOG(L"Can't initialize input devices, input won't work!\n");
	}

	// init game timer , and reset timer immediately
	UHEGameTimer = std::make_unique<UHGameTimer>();
	UHEGameTimer->Reset();

#if WITH_DEBUG
	// init profiler
	UHEProfiler = UHProfiler(UHEGameTimer.get());
	MainThreadProfile = UHProfiler(UHEGameTimer.get());
#endif

	// init default scene after all assets are loaded
	DefaultScene.Initialize(UHEAsset.get(), UHEGraphic.get(), UHEConfig.get(), UHERawInput.get(), UHEGameTimer.get());

	// sync full screen state to graphic
	UHEGraphic->ToggleFullScreen(PresentationSettings.bFullScreen);

	// init renderer
	UHERenderer = std::make_unique<UHDeferredShadingRenderer>(UHEGraphic.get(), UHEAsset.get(), UHEConfig.get(), UHEGameTimer.get());
	if (!UHERenderer->Initialize(&DefaultScene))
	{
		UHE_LOG(L"Can't initialize renderer class!\n");
		return false;
	}
	UHERenderer->SetCurrentScene(&DefaultScene);

	// show window at the end of initialization
	UHEConfig->ApplyPresentationSettings(UHEngineWindow);
	UHEConfig->ApplyWindowStyle(UHWindowInstance, UHEngineWindow);

	// release all CPU data for texture/meshes
	// they shoule be uploaded to GPU at this point
	for (UHTexture2D* Tex : UHEAsset->GetTexture2Ds())
	{
		Tex->ReleaseCPUTextureData();
	}

	for (UHMesh* Mesh : UHEAsset->GetUHMeshes())
	{
		Mesh->ReleaseCPUMeshData();
	}

#if WITH_DEBUG
	// init editor instance
	UHEEditor = std::make_unique<UHEditor>(UHWindowInstance, UHEngineWindow, this, UHEConfig.get(), UHERenderer.get(), UHERawInput.get(), &UHEProfiler);
#endif

	bIsInitialized = true;
	return true;
}

void UHEngine::ReleaseEngine()
{
	UH_SAFE_RELEASE(UHERenderer);
	UHERenderer.reset();

	DefaultScene.Release();

	UHERawInput.reset();
	UHEGameTimer.reset();

	UH_SAFE_RELEASE(UHEAsset);
	UHEAsset.reset();
	UHEConfig.reset();

	UH_SAFE_RELEASE(UHEGraphic);
	UHEGraphic.reset();

#if WITH_DEBUG
	UHEEditor.reset();
#endif
}

bool UHEngine::IsEngineInitialized()
{
	return bIsInitialized;
}

// engine updates
void UHEngine::Update()
{
#if WITH_DEBUG
	MainThreadProfile.Begin();
#endif

	// timer tick
	UHEGameTimer->Tick();

	// update scripts
	for (const auto Script : UHGameScripts)
	{
		Script.second->OnEngineUpdate(UHEGameTimer.get());
	}

	// full screen toggling
	if (UHERawInput->IsKeyHold(VK_MENU) && UHERawInput->IsKeyUp(VK_RETURN))
	{
		ToggleFullScreen();
	}

	// TAA toggling
	if (UHERawInput->IsKeyHold(VK_CONTROL) && UHERawInput->IsKeyUp('t'))
	{
		UHEConfig->ToggleTAA();
	}

	// vsync toggling
	if (UHERawInput->IsKeyHold(VK_CONTROL) && UHERawInput->IsKeyUp('v'))
	{
		UHEConfig->ToggleVsync();
		SetResizeReason(ToggleVsync);
	}

	if (EngineResizeReason != UHEngineResizeReason::NotResizing)
	{
		ResizeEngine();
		EngineResizeReason = UHEngineResizeReason::NotResizing;
	}

	// update scene
	DefaultScene.Update();

	// update renderer, reset if it's need to be reset (device lost..etc)
	if (UHERenderer->IsNeedReset())
	{
		// GFX needs to be reset also
		UHERenderer->Release();
		UHEGraphic->Release();
		UHEGraphic->InitGraphics(UHEngineWindow);
		UHERenderer->Reset();
	}
	UHERenderer->Update();

	// cache input state of this frame
	UHERawInput->ResetMouseData();
	UHERawInput->CacheKeyStates();

	GFrameNumber = (GFrameNumber + 1) & UINT32_MAX;

#if WITH_DEBUG
	MainThreadProfile.End();
#endif
}

// engine render loop
void UHEngine::RenderLoop()
{
	UHERenderer->NotifyRenderThread();
}

void UHEngine::ResizeEngine()
{
	UHEGraphic->ResizeSwapChain();

	// doesn't need to recreate rendering buffers if resolution is not changed
	if (EngineResizeReason != UHEngineResizeReason::NewResolution)
	{
		UHERenderer->Resize();
	}
}

void UHEngine::ToggleFullScreen()
{
	UHEConfig->ToggleFullScreen();
	UHEConfig->ApplyWindowStyle(UHWindowInstance, UHEngineWindow);
	UHEGraphic->ToggleFullScreen(UHEConfig->PresentationSetting().bFullScreen);
}

void UHEngine::SetResizeReason(UHEngineResizeReason InFlag)
{
	EngineResizeReason = InFlag;
}

UHRawInput* UHEngine::GetRawInput() const
{
	if (UHERawInput)
	{
		return UHERawInput.get();
	}
	return nullptr;
}

void UHEngine::BeginFPSLimiter()
{
	FrameBeginTime = UHEGameTimer->GetTime();
}

void UHEngine::EndFPSLimiter()
{
	// if FPSLimit is 0, don't limit it
	if (UHEConfig->EngineSetting().FPSLimit < std::numeric_limits<float>::epsilon())
	{
		return;
	}

	int64_t FrameEndTime = UHEGameTimer->GetTime();
	float Duration = static_cast<float>((FrameEndTime - FrameBeginTime) * UHEGameTimer->GetSecondsPerCount()) * 1000.0f;
	float DesiredDuration = (1.0f / UHEConfig->EngineSetting().FPSLimit) * 1000.0f;

	if (DesiredDuration > Duration)
	{
		Sleep(static_cast<DWORD>(DesiredDuration - Duration));
	}
}

#if WITH_DEBUG

UHEditor* UHEngine::GetEditor() const
{
	return UHEEditor.get();
}

void UHEngine::BeginProfile()
{
	UHEProfiler.Begin();
}

void UHEngine::EndProfile()
{
	UHEProfiler.End();

	// show fps on window caption
	float FPS = UHEProfiler.CalculateFPS();
	std::wstringstream FPSStream;
	FPSStream << std::fixed << std::setprecision(2) << FPS;

	std::wstring NewCaption = WindowCaption + L" - " + FPSStream.str() + L" FPS";
	SetWindowText(UHEngineWindow, NewCaption.c_str());

	// sync stats
	UHStatistics& Stats = UHEProfiler.GetStatistics();
	Stats.MainThreadTime = MainThreadProfile.GetDiff() * 1000.0f;
	Stats.RenderThreadTime = UHERenderer->GetRenderThreadTime();
	Stats.TotalTime = UHEProfiler.GetDiff() * 1000.0f;
	Stats.FPS = FPS;
	Stats.DrawCallCount = UHERenderer->GetDrawCallCount();
	Stats.PSOCount = static_cast<int32_t>(UHEGraphic->StatePools.size());
	Stats.ShaderCount = static_cast<int32_t>(UHEGraphic->ShaderPools.size());
	Stats.RTCount = static_cast<int32_t>(UHEGraphic->RTPools.size());
	Stats.SamplerCount = static_cast<int32_t>(UHEGraphic->SamplerPools.size());
	Stats.TextureCount = static_cast<int32_t>(UHEGraphic->Texture2DPools.size());
	Stats.TextureCubeCount = static_cast<int32_t>(UHEGraphic->TextureCubePools.size());
	Stats.MateralCount = static_cast<int32_t>(UHEGraphic->MaterialPools.size());
	Stats.GPUTimes = UHERenderer->GetGPUTimes();
}

#endif