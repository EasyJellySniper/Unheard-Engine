#include "Config.h"
#include <fstream>
#include "../Classes/Utility.h"
#include "../../Resource.h"
#include "../../framework.h"
#include "../../UnheardEngine.h"

UHConfigManager::UHConfigManager()
	:  PresentationSettings(UHPresentationSettings())
{

}

// load config
void UHConfigManager::LoadConfig()
{
	std::ifstream FileIn(L"UHESettings.ini", std::ios::in);
	if (FileIn.is_open())
	{
		if (UHUtilities::SeekINISection(FileIn, "PresentationSettings"))
		{
			PresentationSettings.WindowWidth = UHUtilities::ReadINIData<int32_t>(FileIn, "WindowWidth");
			PresentationSettings.WindowHeight = UHUtilities::ReadINIData<int32_t>(FileIn, "WindowHeight");
			PresentationSettings.bVsync = UHUtilities::ReadINIData<int32_t>(FileIn, "bVsync");
			PresentationSettings.bFullScreen = UHUtilities::ReadINIData<int32_t>(FileIn, "bFullScreen");
		}

		if (UHUtilities::SeekINISection(FileIn, "EngineSettings"))
		{
			EngineSettings.DefaultCameraMoveSpeed = UHUtilities::ReadINIData<float>(FileIn, "DefaultCameraMoveSpeed");
			EngineSettings.MouseRotationSpeed = UHUtilities::ReadINIData<float>(FileIn, "MouseRotationSpeed");
			EngineSettings.ForwardKey = UHUtilities::ReadINIData<char>(FileIn, "ForwardKey");
			EngineSettings.BackKey = UHUtilities::ReadINIData<char>(FileIn, "BackKey");
			EngineSettings.LeftKey = UHUtilities::ReadINIData<char>(FileIn, "LeftKey");
			EngineSettings.RightKey = UHUtilities::ReadINIData<char>(FileIn, "RightKey");
			EngineSettings.DownKey = UHUtilities::ReadINIData<char>(FileIn, "DownKey");
			EngineSettings.UpKey = UHUtilities::ReadINIData<char>(FileIn, "UpKey");
			EngineSettings.FPSLimit = UHUtilities::ReadINIData<float>(FileIn, "FPSLimit");
			EngineSettings.MeshBufferMemoryBudgetMB = UHUtilities::ReadINIData<float>(FileIn, "MeshBufferMemoryBudgetMB");
			EngineSettings.ImageMemoryBudgetMB = UHUtilities::ReadINIData<float>(FileIn, "ImageMemoryBudgetMB");

			// clamp a few parameters
			EngineSettings.MeshBufferMemoryBudgetMB = std::clamp(EngineSettings.MeshBufferMemoryBudgetMB, 0.1f, std::numeric_limits<float>::max());
			EngineSettings.ImageMemoryBudgetMB = std::clamp(EngineSettings.ImageMemoryBudgetMB, 256.0f, std::numeric_limits<float>::max());
		}

		if (UHUtilities::SeekINISection(FileIn, "RenderingSettings"))
		{
			RenderingSettings.RenderWidth = UHUtilities::ReadINIData<int32_t>(FileIn, "RenderWidth");
			RenderingSettings.RenderHeight = UHUtilities::ReadINIData<int32_t>(FileIn, "RenderHeight");
			RenderingSettings.bTemporalAA = UHUtilities::ReadINIData<int32_t>(FileIn, "bTemporalAA");
			RenderingSettings.bEnableRayTracing = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableRayTracing");
			RenderingSettings.bEnableGPULabeling = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableGPULabeling");
			RenderingSettings.bEnableLayerValidation = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableLayerValidation");
			RenderingSettings.bEnableGPUTiming = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableGPUTiming");
			RenderingSettings.bEnableDepthPrePass = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableDepthPrePass");
			RenderingSettings.bParallelSubmission = UHUtilities::ReadINIData<int32_t>(FileIn, "bParallelSubmission");
			RenderingSettings.ParallelThreads = UHUtilities::ReadINIData<int32_t>(FileIn, "ParallelThreads");
			RenderingSettings.RTDirectionalShadowQuality = UHUtilities::ReadINIData<int32_t>(FileIn, "RTDirectionalShadowQuality");
			RenderingSettings.bEnableAsyncCompute = UHUtilities::ReadINIData<int32_t>(FileIn, "bEnableAsyncCompute");

			// clamp a few parameters
			RenderingSettings.RenderWidth = std::clamp(RenderingSettings.RenderWidth, 480, 16384);
			RenderingSettings.RenderHeight = std::clamp(RenderingSettings.RenderHeight, 480, 16384);
			RenderingSettings.ParallelThreads = std::clamp(RenderingSettings.ParallelThreads, 0, 16);
		}
	}
	FileIn.close();
}

// save config
void UHConfigManager::SaveConfig(HWND InWindow)
{
	std::ofstream FileOut(L"UHESettings.ini", std::ios::out);
	if (FileOut.is_open())
	{
		UHUtilities::WriteINISection(FileOut, "PresentationSettings");
		UHUtilities::WriteINIData(FileOut, "WindowWidth", PresentationSettings.WindowWidth);
		UHUtilities::WriteINIData(FileOut, "WindowHeight", PresentationSettings.WindowHeight);
		UHUtilities::WriteINIData(FileOut, "bVsync", PresentationSettings.bVsync);
		UHUtilities::WriteINIData(FileOut, "bFullScreen", PresentationSettings.bFullScreen);
		FileOut << std::endl;

		UHUtilities::WriteINISection(FileOut, "EngineSettings");
		UHUtilities::WriteINIData(FileOut, "DefaultCameraMoveSpeed", EngineSettings.DefaultCameraMoveSpeed);
		UHUtilities::WriteINIData(FileOut, "MouseRotationSpeed", EngineSettings.MouseRotationSpeed);
		UHUtilities::WriteINIData(FileOut, "ForwardKey", EngineSettings.ForwardKey);
		UHUtilities::WriteINIData(FileOut, "BackKey", EngineSettings.BackKey);
		UHUtilities::WriteINIData(FileOut, "LeftKey", EngineSettings.LeftKey);
		UHUtilities::WriteINIData(FileOut, "RightKey", EngineSettings.RightKey);
		UHUtilities::WriteINIData(FileOut, "DownKey", EngineSettings.DownKey);
		UHUtilities::WriteINIData(FileOut, "UpKey", EngineSettings.UpKey);
		UHUtilities::WriteINIData(FileOut, "FPSLimit", EngineSettings.FPSLimit);
		UHUtilities::WriteINIData(FileOut, "MeshBufferMemoryBudgetMB", EngineSettings.MeshBufferMemoryBudgetMB);
		UHUtilities::WriteINIData(FileOut, "ImageMemoryBudgetMB", EngineSettings.ImageMemoryBudgetMB);
		FileOut << std::endl;

		UHUtilities::WriteINISection(FileOut, "RenderingSettings");
		UHUtilities::WriteINIData(FileOut, "RenderWidth", RenderingSettings.RenderWidth);
		UHUtilities::WriteINIData(FileOut, "RenderHeight", RenderingSettings.RenderHeight);
		UHUtilities::WriteINIData(FileOut, "bTemporalAA", RenderingSettings.bTemporalAA);
		UHUtilities::WriteINIData(FileOut, "bEnableRayTracing", RenderingSettings.bEnableRayTracing);
		UHUtilities::WriteINIData(FileOut, "bEnableGPULabeling", RenderingSettings.bEnableGPULabeling);
		UHUtilities::WriteINIData(FileOut, "bEnableLayerValidation", RenderingSettings.bEnableLayerValidation);
		UHUtilities::WriteINIData(FileOut, "bEnableGPUTiming", RenderingSettings.bEnableGPUTiming);
		UHUtilities::WriteINIData(FileOut, "bEnableDepthPrePass", RenderingSettings.bEnableDepthPrePass);
		UHUtilities::WriteINIData(FileOut, "bParallelSubmission", RenderingSettings.bParallelSubmission);
		UHUtilities::WriteINIData(FileOut, "ParallelThreads", RenderingSettings.ParallelThreads);
		UHUtilities::WriteINIData(FileOut, "RTDirectionalShadowQuality", RenderingSettings.RTDirectionalShadowQuality);
		UHUtilities::WriteINIData(FileOut, "bEnableAsyncCompute", RenderingSettings.bEnableAsyncCompute);
	}
	FileOut.close();
}

void UHConfigManager::ApplyPresentationSettings(HWND InWindow)
{
	if (PresentationSettings.WindowWidth != 0 && PresentationSettings.WindowHeight != 0)
	{
		SetWindowPos(InWindow, nullptr, 0, 0, PresentationSettings.WindowWidth, PresentationSettings.WindowHeight, 0);
	}
	else
	{
		// calc window size if it's first time launch
		RECT Rect;
		if (GetWindowRect(InWindow, &Rect))
		{
			PresentationSettings.WindowWidth = Rect.right - Rect.left;
			PresentationSettings.WindowHeight = Rect.bottom - Rect.top;
		}
	}
}

void UHConfigManager::ApplyWindowStyle(HINSTANCE InInstance, HWND InWindow)
{
	// apply window style depending on full screen flag
	if (PresentationSettings.bFullScreen)
	{
		// simply use desktop size as fullscreen size
		HWND DesktopWindow = GetDesktopWindow();
		RECT Rect;
		int32_t DesktopWidth = 0;
		int32_t DesktopHeight = 0;

		if (GetWindowRect(DesktopWindow, &Rect))
		{
			DesktopWidth = Rect.right - Rect.left;
			DesktopHeight = Rect.bottom - Rect.top;
		}

		SetWindowLongPtr(InWindow, GWL_STYLE, WS_POPUP);
		SetWindowPos(InWindow, nullptr, 0, 0, DesktopWidth, DesktopHeight, 0);
		SetMenu(InWindow, nullptr);
		ShowWindow(InWindow, SW_SHOW);
	}
	else
	{
		// recover to window
		SetWindowLongPtr(InWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(InWindow, nullptr, 0, 0, PresentationSettings.WindowWidth, PresentationSettings.WindowHeight, 0);

	#if WITH_DEBUG
		// recover menu and icon with debug mode only
		HICON Icon = LoadIcon(InInstance, MAKEINTRESOURCE(IDI_UNHEARDENGINE));
		SendMessage(InWindow, WM_SETICON, ICON_SMALL, (LPARAM)Icon);

		HMENU Menu = LoadMenu(InInstance, MAKEINTRESOURCE(IDC_UNHEARDENGINE));
		SetMenu(InWindow, Menu);
	#endif

		ShowWindow(InWindow, SW_SHOW);
	}
}

// toggle TAA
void UHConfigManager::ToggleTAA()
{
	RenderingSettings.bTemporalAA = !RenderingSettings.bTemporalAA;
}

void UHConfigManager::ToggleVsync()
{
	PresentationSettings.bVsync = !PresentationSettings.bVsync;
}

UHPresentationSettings& UHConfigManager::PresentationSetting()
{
	return PresentationSettings;
}

UHEngineSettings& UHConfigManager::EngineSetting()
{
	return EngineSettings;
}

UHRenderingSettings& UHConfigManager::RenderingSetting()
{
	return RenderingSettings;
}

void UHConfigManager::ToggleFullScreen()
{
	PresentationSettings.bFullScreen = !PresentationSettings.bFullScreen;
}