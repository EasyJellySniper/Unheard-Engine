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

bool UHEngine::InitEngine(HINSTANCE Instance, HWND EngineWindow)
{
	// set affinity of current thread (main thread)
	SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << GMainThreadAffinity);

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

	// import assets, texture needs graphic interface
	UHEAsset->ImportTextures(UHEGraphic.get());
	UHEAsset->ImportCubemaps(UHEGraphic.get());
	UHEAsset->ImportMeshes();
	UHEAsset->ImportMaterials(UHEGraphic.get());

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
	MainThreadProfile = UHProfiler(UHEGameTimer.get());

	// sync full screen state to graphic
	UHEGraphic->ToggleFullScreen(PresentationSettings.bFullScreen);

	// create scene instance, initialize anyway in case there are script-generated components
	CurrentScene = MakeUnique<UHScene>();
	CurrentScene->Initialize(UHEAsset.get(), UHEGraphic.get(), UHEConfig.get(), UHERawInput.get(), UHEGameTimer.get());

	// create renderer
	UHERenderer = MakeUnique<UHDeferredShadingRenderer>(UHEGraphic.get(), UHEAsset.get(), UHEConfig.get(), UHEGameTimer.get());
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG(L"Can't initialize renderer class!\n");
	}
	UHERenderer->SetCurrentScene(CurrentScene.get());

#if WITH_EDITOR
	// init editor instance
	UHEEditor = MakeUnique<UHEditor>(UHWindowInstance, UHEngineWindow, this, UHEConfig.get(), UHERenderer.get(), UHERawInput.get(), &UHEProfiler
		, UHEAsset.get(), UHEGraphic.get());
#endif

	bIsInitialized = true;
	for (const auto& Script : UHGameScripts)
	{
		Script.second->OnEngineInitialized(this);
	}

	// show window at the end of initialization
	UHEConfig->ApplyPresentationSettings(UHEngineWindow);
	UHEConfig->ApplyWindowStyle(UHWindowInstance, UHEngineWindow);

	// @TODO: Think better release workflow
#if WITH_RELEASE
	// release all CPU data for texture/meshes for ship build
	// they should be uploaded to GPU at this point
	for (UHTexture2D* Tex : UHEAsset->GetTexture2Ds())
	{
		Tex->ReleaseCPUTextureData();
}

	for (UHTextureCube* Cube : UHEAsset->GetCubemaps())
	{
		Cube->ReleaseCPUData();
	}

	for (UHMesh* Mesh : UHEAsset->GetUHMeshes())
	{
		Mesh->ReleaseCPUMeshData();
	}
#endif

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
}

bool UHEngine::IsEngineInitialized()
{
	return bIsInitialized;
}

// engine updates
void UHEngine::Update()
{
	UHProfilerScope Profiler(&MainThreadProfile);

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
		SetResizeReason(ToggleVsync);
	}

	// update scene
	CurrentScene->Update();

	// update renderer, reset if it's need to be reset (device lost..etc)
#if WITH_RELEASE
	UHERenderer->WaitPreviousRenderTask();
#endif

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

	GFrameNumber++;
}

// engine render loop
void UHEngine::RenderLoop()
{
	UHERenderer->NotifyRenderThread();
}

void UHEngine::ResizeEngine()
{
	UHEGraphic->ResizeSwapChain();

	// resize GBuffer if it touches resolution
	if (EngineResizeReason == UHEngineResizeReason::NewResolution)
	{
		UHERenderer->Resize();
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
	float Duration = static_cast<float>((FrameEndTime - FrameBeginTime) * UHEGameTimer->GetSecondsPerCount());
	float DesiredDuration = (1.0f / UHEConfig->EngineSetting().FPSLimit);

	if (DesiredDuration > Duration)
	{
		int64_t WaitDuration = static_cast<int64_t>((DesiredDuration - Duration) / UHEGameTimer->GetSecondsPerCount());
		while (UHEGameTimer->GetTime() <= WaitDuration + FrameEndTime);
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

	// recreate scene when loading
	UH_SAFE_RELEASE(CurrentScene);
	CurrentScene = MakeUnique<UHScene>();

	// load scene file
	std::ifstream FileIn(InputPath.string().c_str(), std::ios::in | std::ios::binary);
	CurrentScene->OnLoad(FileIn);
	FileIn.close();

	// post load behavior and init scene
	CurrentScene->OnPostLoad(UHEAsset.get());
	CurrentScene->Initialize(UHEAsset.get(), UHEGraphic.get(), UHEConfig.get(), UHERawInput.get(), UHEGameTimer.get());

	// re-initialize renderer
	UH_SAFE_RELEASE(UHERenderer);
	if (!UHERenderer->Initialize(CurrentScene.get()))
	{
		UHE_LOG(L"Can't initialize renderer class!\n");
	}
	UHERenderer->SetCurrentScene(CurrentScene.get());

#if WITH_EDITOR
	UHEEditor->RefreshWorldDialog();
#endif
}

#if WITH_EDITOR

UHEditor* UHEngine::GetEditor() const
{
	return UHEEditor.get();
}

UHGraphic* UHEngine::GetGfx() const
{
	return UHEGraphic.get();
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
	Stats.MainThreadTime = MainThreadProfile.GetDiff() * 1000.0f;
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

	Stats.DrawCallCount = UHERenderer->GetDrawCallCount();
	Stats.PSOCount = static_cast<int32_t>(UHEGraphic->StatePools.size());
	Stats.ShaderCount = static_cast<int32_t>(UHEGraphic->ShaderPools.size());
	Stats.RTCount = static_cast<int32_t>(UHEGraphic->RTPools.size());
	Stats.SamplerCount = static_cast<int32_t>(UHEGraphic->SamplerPools.size());
	Stats.TextureCount = static_cast<int32_t>(UHEGraphic->Texture2DPools.size());
	Stats.TextureCubeCount = static_cast<int32_t>(UHEGraphic->TextureCubePools.size());
	Stats.MateralCount = static_cast<int32_t>(UHEGraphic->MaterialPools.size());
	Stats.SetGPUTimes(UHERenderer->GetGPUTimes());
}

#endif