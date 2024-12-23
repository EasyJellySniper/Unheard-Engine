#include "Config.h"
#include <fstream>
#include "../Classes/Utility.h"
#include "../../Resource.h"
#include "../../framework.h"
#include "../../UnheardEngine.h"
#include "../CoreGlobals.h"

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
		// presentation settings
		std::string Section = "PresentationSettings";
		{
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "WindowWidth", PresentationSettings.WindowWidth);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "WindowHeight", PresentationSettings.WindowHeight);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bVsync", PresentationSettings.bVsync);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bFullScreen", PresentationSettings.bFullScreen);
		}

		// engine settings
		Section = "EngineSettings";
		{
			UHUtilities::ReadINIData<float>(FileIn, Section, "DefaultCameraMoveSpeed", EngineSettings.DefaultCameraMoveSpeed);
			UHUtilities::ReadINIData<float>(FileIn, Section, "MouseRotationSpeed", EngineSettings.MouseRotationSpeed);
			UHUtilities::ReadINIData<char>(FileIn, Section, "ForwardKey", EngineSettings.ForwardKey);
			UHUtilities::ReadINIData<char>(FileIn, Section, "BackKey", EngineSettings.BackKey);
			UHUtilities::ReadINIData<char>(FileIn, Section, "LeftKey", EngineSettings.LeftKey);
			UHUtilities::ReadINIData<char>(FileIn, Section, "RightKey", EngineSettings.RightKey);
			UHUtilities::ReadINIData<char>(FileIn, Section, "DownKey", EngineSettings.DownKey);
			UHUtilities::ReadINIData<char>(FileIn, Section, "UpKey", EngineSettings.UpKey);
			UHUtilities::ReadINIData<float>(FileIn, Section, "FPSLimit", EngineSettings.FPSLimit);
			UHUtilities::ReadINIData<float>(FileIn, Section, "MeshBufferMemoryBudgetMB", EngineSettings.MeshBufferMemoryBudgetMB);
			UHUtilities::ReadINIData<float>(FileIn, Section, "ImageMemoryBudgetMB", EngineSettings.ImageMemoryBudgetMB);

			// clamp a few parameters
			EngineSettings.MeshBufferMemoryBudgetMB = std::clamp(EngineSettings.MeshBufferMemoryBudgetMB, 0.1f, std::numeric_limits<float>::max());
			EngineSettings.ImageMemoryBudgetMB = std::clamp(EngineSettings.ImageMemoryBudgetMB, 256.0f, std::numeric_limits<float>::max());
		}

		// rendering settings
		Section = "RenderingSettings";
		{
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "RenderWidth", RenderingSettings.RenderWidth);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "RenderHeight", RenderingSettings.RenderHeight);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bTemporalAA", RenderingSettings.bTemporalAA);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableRayTracing", RenderingSettings.bEnableRayTracing);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableGPULabeling", RenderingSettings.bEnableGPULabeling);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableLayerValidation", RenderingSettings.bEnableLayerValidation);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableGPUTiming", RenderingSettings.bEnableGPUTiming);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableDepthPrePass", RenderingSettings.bEnableDepthPrePass);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "ParallelThreads", RenderingSettings.ParallelThreads);
			UHUtilities::ReadINIData<float>(FileIn, Section, "RTCullingRadius", RenderingSettings.RTCullingRadius);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "RTShadowQuality", RenderingSettings.RTShadowQuality);
			UHUtilities::ReadINIData<float>(FileIn, Section, "RTShadowTMax", RenderingSettings.RTShadowTMax);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "RTReflectionQuality", RenderingSettings.RTReflectionQuality);
			UHUtilities::ReadINIData<float>(FileIn, Section, "RTReflectionTMax", RenderingSettings.RTReflectionTMax);
			UHUtilities::ReadINIData<float>(FileIn, Section, "RTReflectionSmoothCutoff", RenderingSettings.RTReflectionSmoothCutoff);
			UHUtilities::ReadINIData<float>(FileIn, Section, "FinalReflectionStrength", RenderingSettings.FinalReflectionStrength);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableAsyncCompute", RenderingSettings.bEnableAsyncCompute);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableHDR", RenderingSettings.bEnableHDR);
			UHUtilities::ReadINIData<bool>(FileIn, Section, "bEnableHardwareOcclusion", RenderingSettings.bEnableHardwareOcclusion);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "OcclusionTriangleThreshold", RenderingSettings.OcclusionTriangleThreshold);
			UHUtilities::ReadINIData<float>(FileIn, Section, "HDRWhitePaperNits", RenderingSettings.HDRWhitePaperNits);
			UHUtilities::ReadINIData<float>(FileIn, Section, "HDRContrast", RenderingSettings.HDRContrast);
			UHUtilities::ReadINIData<float>(FileIn, Section, "GammaCorrection", RenderingSettings.GammaCorrection);
			UHUtilities::ReadINIData<int32_t>(FileIn, Section, "PCSSKernal", RenderingSettings.PCSSKernal);
			UHUtilities::ReadINIData<float>(FileIn, Section, "PCSSMinPenumbra", RenderingSettings.PCSSMinPenumbra);
			UHUtilities::ReadINIData<float>(FileIn, Section, "PCSSMaxPenumbra", RenderingSettings.PCSSMaxPenumbra);
			UHUtilities::ReadINIData<float>(FileIn, Section, "PCSSBlockerDistScale", RenderingSettings.PCSSBlockerDistScale);

			// clamp a few parameters
			RenderingSettings.RenderWidth = std::clamp(RenderingSettings.RenderWidth, 480, 16384);
			RenderingSettings.RenderHeight = std::clamp(RenderingSettings.RenderHeight, 480, 16384);
			RenderingSettings.ParallelThreads = std::clamp(RenderingSettings.ParallelThreads, 0, (int32_t)GMaxWorkerThreads);

			RenderingSettings.PCSSKernal = std::clamp(RenderingSettings.PCSSKernal, 1, 3);
			RenderingSettings.PCSSMinPenumbra = std::max(RenderingSettings.PCSSMinPenumbra, 0.0f);
			RenderingSettings.PCSSMaxPenumbra = std::max(RenderingSettings.PCSSMaxPenumbra, 0.0f);
			RenderingSettings.PCSSBlockerDistScale = std::max(RenderingSettings.PCSSBlockerDistScale, 0.0f);
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
		UHUtilities::WriteINIData(FileOut, "ParallelThreads", RenderingSettings.ParallelThreads);
		UHUtilities::WriteINIData(FileOut, "RTCullingRadius", RenderingSettings.RTCullingRadius);
		UHUtilities::WriteINIData(FileOut, "RTShadowQuality", RenderingSettings.RTShadowQuality);
		UHUtilities::WriteINIData(FileOut, "RTShadowTMax", RenderingSettings.RTShadowTMax);
		UHUtilities::WriteINIData(FileOut, "RTReflectionQuality", RenderingSettings.RTReflectionQuality);
		UHUtilities::WriteINIData(FileOut, "RTReflectionTMax", RenderingSettings.RTReflectionTMax);
		UHUtilities::WriteINIData(FileOut, "RTReflectionSmoothCutoff", RenderingSettings.RTReflectionSmoothCutoff);
		UHUtilities::WriteINIData(FileOut, "FinalReflectionStrength", RenderingSettings.FinalReflectionStrength);
		UHUtilities::WriteINIData(FileOut, "bEnableAsyncCompute", RenderingSettings.bEnableAsyncCompute);
		UHUtilities::WriteINIData(FileOut, "bEnableHDR", RenderingSettings.bEnableHDR);
		UHUtilities::WriteINIData(FileOut, "bEnableHardwareOcclusion", RenderingSettings.bEnableHardwareOcclusion);
		UHUtilities::WriteINIData(FileOut, "OcclusionTriangleThreshold", RenderingSettings.OcclusionTriangleThreshold);
		UHUtilities::WriteINIData(FileOut, "HDRWhitePaperNits", RenderingSettings.HDRWhitePaperNits);
		UHUtilities::WriteINIData(FileOut, "HDRContrast", RenderingSettings.HDRContrast);
		UHUtilities::WriteINIData(FileOut, "GammaCorrection", RenderingSettings.GammaCorrection);

		UHUtilities::WriteINIData(FileOut, "PCSSKernal", RenderingSettings.PCSSKernal);
		UHUtilities::WriteINIData(FileOut, "PCSSMinPenumbra", RenderingSettings.PCSSMinPenumbra);
		UHUtilities::WriteINIData(FileOut, "PCSSMaxPenumbra", RenderingSettings.PCSSMaxPenumbra);
		UHUtilities::WriteINIData(FileOut, "PCSSBlockerDistScale", RenderingSettings.PCSSBlockerDistScale);
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
		UpdateWindowSize(InWindow);
	}
}

void UHConfigManager::UpdateWindowSize(HWND InWindow)
{
	// calc window size if it's first time launch
	RECT Rect;
	if (GetWindowRect(InWindow, &Rect))
	{
		PresentationSettings.WindowWidth = Rect.right - Rect.left;
		PresentationSettings.WindowHeight = Rect.bottom - Rect.top;
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

		if (GIsEditor)
		{
			// recover menu and icon with debug mode only
			HICON Icon = LoadIcon(InInstance, MAKEINTRESOURCE(IDI_UNHEARDENGINE));
			SendMessage(InWindow, WM_SETICON, ICON_SMALL, (LPARAM)Icon);

			HMENU Menu = LoadMenu(InInstance, MAKEINTRESOURCE(IDC_UNHEARDENGINE));
			SetMenu(InWindow, Menu);
		}

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