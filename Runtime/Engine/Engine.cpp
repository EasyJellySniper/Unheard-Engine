#include "Engine.h"
#include <fstream>
#include <string>
#include "../CoreGlobals.h"
#include "../Components/GameScript.h"
#include "Runtime/Platform/Client.h"

#if WITH_EDITOR
#include <sstream>
#include <iomanip>
#endif

UHEngine::UHEngine()
	: UHEClient(nullptr)
	, bIsInitialized(false)
	, EngineResizeReason(UHEngineResizeReason::NotResizing)
	, FrameBeginTime(UHClock::time_point())
	, FrameEndTime(UHClock::time_point())
	, DisplayFrequency(60.0f)
	, WaitDuration(UHClock::duration())
	, WaitDurationMS(0.0f)
#if WITH_EDITOR
	, UHEEditor(nullptr)
	, UHEProfiler(nullptr)
#endif
	, WindowCaption(ENGINE_NAME)
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
	UHEConfig->SaveConfig();
}

// NTSC frequencies fixup
float FixupNTSCFrequency(int32_t InFrequency)
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

bool UHEngine::InitEngine(UHClient* InClient)
{
	// set affinity of current thread (main thread)
	// it's confirmed that the affinity setting might introduce stuttering for render/worker threads in UHE
	// so I disable it for main thread too. uncomment this for testing
	//SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << GMainThreadAffinity);
	GMainThreadID = std::this_thread::get_id();
	GCurrentThreadID = GMainThreadID;

	// init asset manager
	UHEAsset = MakeUnique<UHAssetManager>();

	// init graphic 
	const UHPresentationSettings PresentationSettings = UHEConfig->PresentationSetting();
	UHEClient = InClient;
	UHEClient->SetWindowCaption(WindowCaption);

	// cache current monitor refresh rate, also consider the NTSC frequencies
	DisplayFrequency = FixupNTSCFrequency(UHEClient->GetDisplayFrequency());

	UHEGraphic = MakeUnique<UHGraphic>(UHEAsset.get(), UHEConfig.get());
	if (!UHEGraphic->InitGraphics(UHEClient))
	{
		UHE_LOG("Can't initialize graphic class!\n");
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
	UHERawInput = MakeUnique<UHPlatformInput>(UHEClient);
	if (!UHERawInput->InitInput())
	{
		// print a log to remind users that inputs aren't available, and it's okay to proceed
		UHE_LOG("Can't initialize input devices, input won't work!\n");
	}

	// init game timer , and reset timer immediately
	UHEGameTimer = MakeUnique<UHGameTimer>();
	UHEGameTimer->Reset();

#if WITH_EDITOR
	// init profiler
	UHEProfiler = UHProfiler(UHEGameTimer.get());
	EngineUpdateProfile = UHProfiler(UHEGameTimer.get());
#endif

	// sync full screen state to graphic
	UHEGraphic->ToggleFullScreen(PresentationSettings.bFullScreen);

	// create scene instance, initialize anyway in case there are script-generated components
	CurrentScene = MakeUnique<UHScene>();
	CurrentScene->Initialize(this);

	// create renderer
	UHERenderer = MakeUnique<UHDeferredShadingRenderer>(this);
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG("Can't initialize renderer class!\n");
	}

#if WITH_EDITOR
	// init editor instance
	UHEEditor = MakeUnique<UHEditor>(UHEClient, this, &UHEProfiler);
#endif

	bIsInitialized = true;
	for (const auto& Script : UHGameScripts)
	{
		Script.second->OnEngineInitialized(this);
	}

	// show window at the end of initialization
	UHEConfig->ApplyPresentationSettings(UHEClient);
	UHEConfig->ApplyWindowStyle(UHEClient);

	return true;
}

void UHEngine::ReleaseEngine()
{
	UH_SAFE_RELEASE(UHERenderer);
	UHERenderer.reset();

	UH_SAFE_RELEASE(CurrentScene);

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
}

bool UHEngine::IsEngineInitialized()
{
	return bIsInitialized;
}

// engine updates
void UHEngine::Update()
{
#if WITH_EDITOR
	UHProfilerScope Profiler(&EngineUpdateProfile);
#endif

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
	if (UHERawInput->IsKeyHold(UH_ENUM_VALUE(UHSystemKey::Alt)) && UHERawInput->IsKeyUp(UH_ENUM_VALUE(UHSystemKey::Enter)))
	{
		UHEConfig->ToggleFullScreen();
		ToggleFullScreen();
	}

	// TAA toggling
	if (UHERawInput->IsKeyHold(UH_ENUM_VALUE(UHSystemKey::Control)) && UHERawInput->IsKeyUp('t'))
	{
		UHEConfig->ToggleTAA();
	}

	// vsync toggling
	if (UHERawInput->IsKeyHold(UH_ENUM_VALUE(UHSystemKey::Control)) && UHERawInput->IsKeyUp('v'))
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
		UHEGraphic->InitGraphics(UHEClient);
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
	if (EngineResizeReason == UHEngineResizeReason::FromClientCallback
		&& !UHEConfig->PresentationSetting().bFullScreen)
	{
		UHEConfig->UpdateWindowSize(UHEClient);
	}
}

void UHEngine::ToggleFullScreen()
{
	UHEConfig->ApplyWindowStyle(UHEClient);
	UHEGraphic->ToggleFullScreen(UHEConfig->PresentationSetting().bFullScreen);
}

void UHEngine::SetResizeReason(UHEngineResizeReason InFlag)
{
	EngineResizeReason = InFlag;
}

UHPlatformInput* UHEngine::GetRawInput() const
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

UHScene* UHEngine::GetScene() const
{
	return CurrentScene.get();
}

void UHEngine::BeginFPSLimiter()
{
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
	float Duration = std::chrono::duration<float>(FrameEndTime - FrameBeginTime).count();
	const float DesiredDuration = 1.0f / FPSLimit;

	if (DesiredDuration > Duration)
	{
		WaitDuration = std::chrono::duration<float>(DesiredDuration - Duration);
		WaitDurationMS = (DesiredDuration - Duration) * 1000.0f;

		auto TargetTime = FrameEndTime + WaitDuration;

		// sleep if the requested MS is larger than a value
		// this reduces the CPU burning for the busy loop below
		const float SleepThresholdMS = 5.0f;
		if (WaitDurationMS > SleepThresholdMS)
		{
			std::chrono::duration<float, std::milli> SleepMS(WaitDurationMS - SleepThresholdMS);
			std::this_thread::sleep_for(SleepMS);
		}

		// wait the remaining time, busy loop is needed for the best accuracy
		UHClock::time_point StartWaitingTime = UHEGameTimer->GetTime();
		while (true)
		{
			std::this_thread::yield();
			if ((UHEGameTimer->GetTime() - StartWaitingTime) > WaitDuration)
			{
				break;
			}
		}
	}
}

void UHEngine::DisplayFPS()
{
	// FPS count for shipped build
	if (GIsShipping)
	{
		FrameEndTime = UHEGameTimer->GetTime();
		float Duration = std::chrono::duration<float>(FrameEndTime - FrameBeginTime).count();
		DisplayFPSTitle(Duration * 1000.0f);
	}
}

void UHEngine::DisplayFPSTitle(float InDurationMS)
{
	// calc fps from total time, only do this once a second
	static float TimeElasped = 0.0f;
	float GameTime = UHEGameTimer->GetTotalTime();

	if (UHEClient != nullptr && (GameTime - TimeElasped) > 1.0f)
	{
		float FPS = 1000.0f / InDurationMS;
		std::stringstream FPSStream;
		FPSStream << std::fixed << std::setprecision(2) << FPS;

		std::string NewCaption = WindowCaption + " - " + FPSStream.str() + " FPS";
		UHEClient->SetWindowCaption(NewCaption);
		TimeElasped = GameTime;
	}
}

void UHEngine::ResetScene()
{
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
}

void UHEngine::OnSaveScene(std::filesystem::path OutputPath)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	std::ofstream FileOut(OutputPath.generic_string().c_str(), std::ios::out | std::ios::binary);
	CurrentScene->SetName(OutputPath.filename().stem().generic_string());
	CurrentScene->OnSave(FileOut);
	FileOut.close();
}

void UHEngine::OnLoadScene(std::filesystem::path InputPath)
{
	if (!std::filesystem::exists(InputPath))
	{
		UHE_LOG("Scene " + InputPath.generic_string() + " not found!\n");
		return;
	}

	ResetScene();

	// load scene file
	std::ifstream FileIn(InputPath.generic_string().c_str(), std::ios::in | std::ios::binary);
	CurrentScene->OnLoad(FileIn);
	FileIn.close();

	// post load behavior and init scene
	CurrentScene->OnPostLoad(UHEAsset.get());
	CurrentScene->Initialize(this);

	// re-initialize renderer
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG("Can't initialize renderer class!\n");
	}
	UHERenderer->InitRenderingResources();
	UHERenderer->RefreshSkyLight(false);

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

	DisplayFPSTitle(Stats.TotalTime);

	Stats.FPS = 1000.0f / Stats.TotalTime;
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