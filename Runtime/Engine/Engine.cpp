#include "Engine.h"
#include <fstream>
#include <string>
#include "../CoreGlobals.h"
#include "../Components/GameScript.h"

#if WITH_EDITOR
#include <sstream>
#include <iomanip>
#endif

UHEngine::UHEngine()
	: UHEngineWindow(nullptr)
	, UHWindowInstance(nullptr)
	, bIsInitialized(false)
	, EngineResizeReason(UHEngineResizeReason::NotResizing)
	, FrameBeginTime(0)
	, FrameEndTime(0)
	, DisplayFrequency(60.0f)
	, WaitDuration(0)
	, WaitDurationMS(0.0f)
#if WITH_EDITOR
	, UHEEditor(nullptr)
	, UHEProfiler(nullptr)
	, WindowCaption(ENGINE_NAMEW)
#endif
{
	// config manager needs to be initialze as early as possible
	UHEConfig = MakeUnique<UHConfigManager>();
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

// NTSC frequencies fixup
float FixupNTSCFrequency(DWORD InFrequency)
{
	switch (InFrequency)
	{
	case 23:
	case 29:
	case 59:
	case 119:
		return (static_cast<float>(InFrequency) + 1) * 1000.0f / 1001.0f;
	}

	return static_cast<float>(InFrequency);
}

bool UHEngine::InitEngine(HINSTANCE Instance, HWND EngineWindow)
{
	// set affinity of current thread (main thread)
	// it's confirmed that the affinity setting might introduce stuttering for render/worker threads in UHE
	// so I disable it for main thread too. uncomment this for testing
	//SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << GMainThreadAffinity);
	GMainThreadID = std::this_thread::get_id();

	// cache current monitor refresh rate, also consider the NTSC frequencies
	DEVMODE DevMode;
	DevMode.dmSize = sizeof(DEVMODE);
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DevMode);
	DisplayFrequency = FixupNTSCFrequency(DevMode.dmDisplayFrequency);

	// init asset manager
	UHEAsset = MakeUnique<UHAssetManager>();

	// init graphic 
	const UHPresentationSettings PresentationSettings = UHEConfig->PresentationSetting();
	UHEngineWindow = EngineWindow;
	UHWindowInstance = Instance;

	UHEGraphic = MakeUnique<UHGraphic>(UHEAsset.get(), UHEConfig.get());
	if (!UHEGraphic->InitGraphics(UHEngineWindow))
	{
		UHE_LOG(L"Can't initialize graphic class!\n");
		return false;
	}

	UHEAsset->SetGfxCache(UHEGraphic.get());

#if WITH_EDITOR
	// import assets for editor
	UHEAsset->ImportAssets();
#else
	UHEAsset->ImportBuiltInAssets();
#endif

	// init input 
	UHERawInput = MakeUnique<UHRawInput>();
	if (!UHERawInput->InitRawInput())
	{
		// print a log to remind users that inputs aren't available, and it's okay to proceed
		UHE_LOG(L"Can't initialize input devices, input won't work!\n");
	}

	// init game timer , and reset timer immediately
	UHEGameTimer = MakeUnique<UHGameTimer>();
	UHEGameTimer->Reset();

	// init profiler
	UHEProfiler = UHProfiler(UHEGameTimer.get());
	EngineUpdateProfile = UHProfiler(UHEGameTimer.get());

	// sync full screen state to graphic
	UHEGraphic->ToggleFullScreen(PresentationSettings.bFullScreen);

	// create scene instance, initialize anyway in case there are script-generated components
	CurrentScene = MakeUnique<UHScene>();
	CurrentScene->Initialize(this);

	// create renderer
	UHERenderer = MakeUnique<UHDeferredShadingRenderer>(this);
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG(L"Can't initialize renderer class!\n");
	}

#if WITH_EDITOR
	// init editor instance
	UHEEditor = MakeUnique<UHEditor>(UHWindowInstance, UHEngineWindow, this, &UHEProfiler);
#endif

	bIsInitialized = true;
	for (const auto& Script : UHGameScripts)
	{
		Script.second->OnEngineInitialized(this);
	}

	// show window at the end of initialization
	UHEConfig->ApplyPresentationSettings(UHEngineWindow);
	UHEConfig->ApplyWindowStyle(UHWindowInstance, UHEngineWindow);

	FramerateLimitThread = MakeUnique<UHThread>();
	FramerateLimitThread->BeginThread(std::thread(&UHEngine::LimitFramerate, this));

	return true;
}

void UHEngine::ReleaseEngine()
{
	UH_SAFE_RELEASE(UHERenderer);
	UHERenderer.reset();

	CurrentScene->Release();

	UHERawInput.reset();
	UHEGameTimer.reset();

	UH_SAFE_RELEASE(UHEAsset);
	UHEAsset.reset();
	UHEConfig.reset();

#if WITH_EDITOR
	UHEEditor.reset();
#endif

	UH_SAFE_RELEASE(UHEGraphic);
	UHEGraphic.reset();

	FramerateLimitThread->WaitTask();
	FramerateLimitThread->EndThread();
}

bool UHEngine::IsEngineInitialized()
{
	return bIsInitialized;
}

// engine updates
void UHEngine::Update()
{
	UHProfilerScope Profiler(&EngineUpdateProfile);

	// timer tick
	UHEGameTimer->Tick();

	// update scripts
	for (const auto& Script : UHGameScripts)
	{
		if (Script.second->IsEnabled())
		{
			Script.second->OnEngineUpdate(UHEGameTimer->GetDeltaTime());
		}
	}

	// full screen toggling
	if (UHERawInput->IsKeyHold(VK_MENU) && UHERawInput->IsKeyUp(VK_RETURN))
	{
		UHEConfig->ToggleFullScreen();
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
		SetResizeReason(UHEngineResizeReason::ToggleVsync);
	}

	// update scene
	CurrentScene->Update();

	// wait previous render task done before new updates
	if (GIsShipping)
	{
		UHERenderer->WaitPreviousRenderTask();
	}

	if (EngineResizeReason != UHEngineResizeReason::NotResizing)
	{
		ResizeEngine();
		UHERenderer->SetSwapChainReset(true);
		EngineResizeReason = UHEngineResizeReason::NotResizing;
	}
	else
	{
		UHERenderer->SetSwapChainReset(false);
	}

	if (UHERenderer->IsNeedReset())
	{
		// GFX needs to be reset also
		UHERenderer->Release();
		UHEGraphic->Release();
		UHEGraphic->InitGraphics(UHEngineWindow);
		UHERenderer->Reset();
	}

#if WITH_EDITOR
	uint32_t DeltaW = 0;
	uint32_t DeltaH = 0;
	UHEEditor->EvaluateEditorDelta(DeltaW, DeltaH);
	UHERenderer->SetEditorDelta(DeltaW, DeltaH);
#endif
	UHERenderer->Update();

	// cache input state of this frame
	UHERawInput->ResetMouseData();
	UHERawInput->CacheKeyStates();
}

// engine render loop
void UHEngine::RenderLoop()
{
	UHERenderer->NotifyRenderThread();
	GFrameNumber++;
}

void UHEngine::ResizeEngine()
{
	UHEGraphic->ResizeSwapChain();

	// resize GBuffer if it touches resolution
	if (EngineResizeReason == UHEngineResizeReason::NewResolution)
	{
		UHERenderer->Resize();
	}

	// sync the window size if it's window message
	if (EngineResizeReason == UHEngineResizeReason::FromWndMessage
		&& !UHEConfig->PresentationSetting().bFullScreen)
	{
		UHEConfig->UpdateWindowSize(UHEngineWindow);
	}
}

void UHEngine::ToggleFullScreen()
{
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

UHGraphic* UHEngine::GetGfx() const
{
	return UHEGraphic.get();
}

UHGameTimer* UHEngine::GetGameTimer() const
{
	return UHEGameTimer.get();
}

UHAssetManager* UHEngine::GetAssetManager() const
{
	return UHEAsset.get();
}

UHConfigManager* UHEngine::GetConfigManager() const
{
	return UHEConfig.get();
}

UHDeferredShadingRenderer* UHEngine::GetSceneRenderer() const
{
	return UHERenderer.get();
}

void UHEngine::BeginFPSLimiter()
{
	if (GIsShipping)
	{
		FramerateLimitThread->WaitTask();
	}

	FrameBeginTime = UHEGameTimer->GetTime();
}

void UHEngine::EndFPSLimiter()
{
	const float FPSLimit = UHEConfig->EngineSetting().FPSLimit;

	// if FPSLimit is 0, don't limit it
	if (FPSLimit < std::numeric_limits<float>::epsilon())
	{
		return;
	}

	// do not need to limit fps if Vsync is on and limit is > monitor HZ
	if (UHEConfig->PresentationSetting().bVsync && FPSLimit >= DisplayFrequency)
	{
		return;
	}

	FrameEndTime = UHEGameTimer->GetTime();
	const float Duration = static_cast<float>((FrameEndTime - FrameBeginTime) * UHEGameTimer->GetSecondsPerCount());
	const float DesiredDuration = 1.0f / FPSLimit;

	if (DesiredDuration > Duration)
	{
		WaitDuration = static_cast<int64_t>((DesiredDuration - Duration) / UHEGameTimer->GetSecondsPerCount());
		WaitDurationMS = (DesiredDuration - Duration) * 1000.0f;

		FramerateLimitThread->WakeThread();
		if (GIsEditor)
		{
			FramerateLimitThread->WaitTask();
		}
	}
}

void UHEngine::LimitFramerate()
{
	auto Curr = std::chrono::steady_clock::now();
	using FPS100 = std::chrono::duration<int32_t, std::ratio<1, 100>>;
	auto Next = Curr + FPS100{ 1 };
	const float SleepThresholdMS = 10.0f;

	while (true)
	{
		FramerateLimitThread->WaitNotify();

		if (FramerateLimitThread->IsTermindate())
		{
			break;
		}

		// sleep if the requested MS is larger than a value
		if (WaitDurationMS > SleepThresholdMS)
		{
			std::this_thread::sleep_until(Next);
			Next += FPS100{ 1 };
		}

		// wait the remaining time
		while (true)
		{
			std::this_thread::yield();
			if (UHEGameTimer->GetTime() > WaitDuration + FrameEndTime)
			{
				break;
			}
		}

		FramerateLimitThread->NotifyTaskDone();
	}
}

void UHEngine::OnSaveScene(std::filesystem::path OutputPath)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	std::ofstream FileOut(OutputPath.string().c_str(), std::ios::out | std::ios::binary);
	CurrentScene->SetName(OutputPath.filename().stem().string());
	CurrentScene->OnSave(FileOut);
	FileOut.close();
}

void UHEngine::OnLoadScene(std::filesystem::path InputPath)
{
	if (!std::filesystem::exists(InputPath))
	{
		UHE_LOG("Scene " + InputPath.string() + " not found!\n");
		return;
	}

	// wait all previous rendering done
	UHERenderer->WaitPreviousRenderTask();
	UHEGraphic->WaitGPU();
	UH_SAFE_RELEASE(UHERenderer);

	// release assets of previous map for re-import
	// and also reset the shared memory
	if (GIsShipping)
	{
		UHEAsset->Release();
		UHEAsset->ImportBuiltInAssets();
		UHEGraphic->GetImageSharedMemory()->Reset();
		UHEGraphic->GetMeshSharedMemory()->Reset();
	}

	// recreate scene when loading
	UH_SAFE_RELEASE(CurrentScene);
	CurrentScene = MakeUnique<UHScene>();

	// load scene file
	std::ifstream FileIn(InputPath.string().c_str(), std::ios::in | std::ios::binary);
	CurrentScene->OnLoad(FileIn);
	FileIn.close();

	// post load behavior and init scene
	CurrentScene->OnPostLoad(UHEAsset.get());
	CurrentScene->Initialize(this);

	// re-initialize renderer
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG(L"Can't initialize renderer class!\n");
	}
	UHERenderer->InitRenderingResources();

#if WITH_EDITOR
	UHEEditor->RefreshWorldDialog();
#endif
}

#if WITH_EDITOR

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

	// sync stats
	UHStatistics& Stats = UHEProfiler.GetStatistics();
	Stats.EngineUpdateTime = EngineUpdateProfile.GetDiff() * 1000.0f;
	Stats.RenderThreadTime = UHERenderer->GetRenderThreadTime();
	Stats.TotalTime = UHEProfiler.GetDiff() * 1000.0f;

	// calc fps from total time, only do this once a second
	static float TimeElasped = 0.0f;
	float GameTime = UHEGameTimer->GetTotalTime();
	if (GameTime - TimeElasped > 1.0f)
	{
		float FPS = 1000.0f / Stats.TotalTime;
		std::wstringstream FPSStream;
		FPSStream << std::fixed << std::setprecision(2) << FPS;

		std::wstring NewCaption = WindowCaption + L" - " + FPSStream.str() + L" FPS";
		SetWindowText(UHEngineWindow, NewCaption.c_str());
		TimeElasped = GameTime;
		Stats.FPS = FPS;
	}

	Stats.RendererCount = CurrentScene ? static_cast<int32_t>(CurrentScene->GetAllRendererCount()) : 0;
	Stats.DrawCallCount = UHERenderer->GetDrawCallCount();
	Stats.OccludedCallCount = UHERenderer->GetOccludedCallCount();
	Stats.PSOCount = static_cast<int32_t>(UHEGraphic->StatePools.size());
	Stats.ShaderCount = static_cast<int32_t>(UHEGraphic->ShaderPools.size());
	Stats.RTCount = static_cast<int32_t>(UHEGraphic->RTPools.size());
	Stats.SamplerCount = static_cast<int32_t>(UHEGraphic->SamplerPools.size());
	Stats.TextureCount = static_cast<int32_t>(UHEGraphic->Texture2DPools.size());
	Stats.TextureCubeCount = static_cast<int32_t>(UHEGraphic->TextureCubePools.size());
	Stats.MateralCount = static_cast<int32_t>(UHEGraphic->MaterialPools.size());
}

#endif