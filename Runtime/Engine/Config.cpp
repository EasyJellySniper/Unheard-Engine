#include "Config.h"
#include <fstream>
#include "../Classes/Utility.h"
#include "../../Resource.h"
#include "../../framework.h"
#include "../../UnheardEngine.h"
#include "../CoreGlobals.h"

// wrapper for Set/GetUHESetting
#define SET_UHE_SETTING(x,y) SetUHESetting(#x, #y, x.y)
#define GET_UHE_SETTING(x,y) GetUHESetting(#x, #y, x.y)

UHConfigManager::UHConfigManager()
{

}

// load config
void UHConfigManager::LoadConfig()
{
	UHESettings = LoadIniFile("UHESettings.ini");
	
	// presentation settings
	{
		GET_UHE_SETTING(PresentationSettings, WindowWidth);
		GET_UHE_SETTING(PresentationSettings, WindowHeight);
		GET_UHE_SETTING(PresentationSettings, bVsync);
		GET_UHE_SETTING(PresentationSettings, bFullScreen);
	}

	// engine settings
	{
		GET_UHE_SETTING(EngineSettings, DefaultCameraMoveSpeed);
		GET_UHE_SETTING(EngineSettings, MouseRotationSpeed);
		GET_UHE_SETTING(EngineSettings, ForwardKey);
		GET_UHE_SETTING(EngineSettings, BackKey);
		GET_UHE_SETTING(EngineSettings, LeftKey);
		GET_UHE_SETTING(EngineSettings, RightKey);
		GET_UHE_SETTING(EngineSettings, DownKey);
		GET_UHE_SETTING(EngineSettings, UpKey);
		GET_UHE_SETTING(EngineSettings, FPSLimit);
		GET_UHE_SETTING(EngineSettings, MeshBufferMemoryBudgetMB);
		GET_UHE_SETTING(EngineSettings, ImageMemoryBudgetMB);

		// clamp a few parameters
		EngineSettings.MeshBufferMemoryBudgetMB = std::clamp(EngineSettings.MeshBufferMemoryBudgetMB, 0.1f, std::numeric_limits<float>::max());
		EngineSettings.ImageMemoryBudgetMB = std::clamp(EngineSettings.ImageMemoryBudgetMB, 256.0f, std::numeric_limits<float>::max());
	}

	// rendering settings
	{
		GET_UHE_SETTING(RenderingSettings, RenderWidth);
		GET_UHE_SETTING(RenderingSettings, RenderHeight);
		GET_UHE_SETTING(RenderingSettings, bTemporalAA);
		GET_UHE_SETTING(RenderingSettings, bEnableRayTracing);
		GET_UHE_SETTING(RenderingSettings, bEnableGPULabeling);
		GET_UHE_SETTING(RenderingSettings, bEnableLayerValidation);
		GET_UHE_SETTING(RenderingSettings, bEnableGPUTiming);
		GET_UHE_SETTING(RenderingSettings, bEnableDepthPrePass);
		GET_UHE_SETTING(RenderingSettings, ParallelSubmitters);
		GET_UHE_SETTING(RenderingSettings, RTCullingRadius);
		GET_UHE_SETTING(RenderingSettings, RTShadowQuality);
		GET_UHE_SETTING(RenderingSettings, RTShadowTMax);
		GET_UHE_SETTING(RenderingSettings, RTReflectionQuality);
		GET_UHE_SETTING(RenderingSettings, RTReflectionTMax);
		GET_UHE_SETTING(RenderingSettings, RTReflectionSmoothCutoff);
		GET_UHE_SETTING(RenderingSettings, FinalReflectionStrength);
		GET_UHE_SETTING(RenderingSettings, bDenoiseRayTracing);
		GET_UHE_SETTING(RenderingSettings, bEnableAsyncCompute);
		GET_UHE_SETTING(RenderingSettings, bEnableHDR);
		GET_UHE_SETTING(RenderingSettings, OcclusionTriangleThreshold);
		GET_UHE_SETTING(RenderingSettings, HDRWhitePaperNits);
		GET_UHE_SETTING(RenderingSettings, HDRContrast);
		GET_UHE_SETTING(RenderingSettings, GammaCorrection);
		GET_UHE_SETTING(RenderingSettings, PCSSKernal);
		GET_UHE_SETTING(RenderingSettings, PCSSMinPenumbra);
		GET_UHE_SETTING(RenderingSettings, PCSSMaxPenumbra);
		GET_UHE_SETTING(RenderingSettings, PCSSBlockerDistScale);
		GET_UHE_SETTING(RenderingSettings, bEnableHardwareOcclusion);
		GET_UHE_SETTING(RenderingSettings, SelectedGpuName);
		GET_UHE_SETTING(RenderingSettings, bEnableRTShadow);
		GET_UHE_SETTING(RenderingSettings, bEnableRTReflection);
		GET_UHE_SETTING(RenderingSettings, bEnableRTIndirectLighting);
		GET_UHE_SETTING(RenderingSettings, RTIndirectLightScale);
		GET_UHE_SETTING(RenderingSettings, RTIndirectLightFadeDistance);
		GET_UHE_SETTING(RenderingSettings, RTIndirectTMax);
		GET_UHE_SETTING(RenderingSettings, RTIndirectOcclusionDistance);

		// clamp a few parameters
		RenderingSettings.RenderWidth = std::clamp(RenderingSettings.RenderWidth, 480, 16384);
		RenderingSettings.RenderHeight = std::clamp(RenderingSettings.RenderHeight, 480, 16384);
		RenderingSettings.ParallelSubmitters = std::clamp(RenderingSettings.ParallelSubmitters, 0, (int32_t)GMaxParallelSubmitters);

		RenderingSettings.PCSSKernal = std::clamp(RenderingSettings.PCSSKernal, 1, 3);
		RenderingSettings.PCSSMinPenumbra = std::max(RenderingSettings.PCSSMinPenumbra, 0.0f);
		RenderingSettings.PCSSMaxPenumbra = std::max(RenderingSettings.PCSSMaxPenumbra, 0.0f);
		RenderingSettings.PCSSBlockerDistScale = std::max(RenderingSettings.PCSSBlockerDistScale, 0.0f);
	}
}

// this function will apply config from all kinds of settings to UHIniData array, mainly for saving purpose later
void UHConfigManager::ApplyConfig()
{
	// presentation settings
	{
		SET_UHE_SETTING(PresentationSettings, WindowWidth);
		SET_UHE_SETTING(PresentationSettings, WindowHeight);
		SET_UHE_SETTING(PresentationSettings, bVsync);
		SET_UHE_SETTING(PresentationSettings, bFullScreen);
	}

	// engine settings
	{
		SET_UHE_SETTING(EngineSettings, DefaultCameraMoveSpeed);
		SET_UHE_SETTING(EngineSettings, MouseRotationSpeed);
		SET_UHE_SETTING(EngineSettings, ForwardKey);
		SET_UHE_SETTING(EngineSettings, BackKey);
		SET_UHE_SETTING(EngineSettings, LeftKey);
		SET_UHE_SETTING(EngineSettings, RightKey);
		SET_UHE_SETTING(EngineSettings, DownKey);
		SET_UHE_SETTING(EngineSettings, UpKey);
		SET_UHE_SETTING(EngineSettings, FPSLimit);
		SET_UHE_SETTING(EngineSettings, MeshBufferMemoryBudgetMB);
		SET_UHE_SETTING(EngineSettings, ImageMemoryBudgetMB);
	}

	// rendering settings
	{
		SET_UHE_SETTING(RenderingSettings, RenderWidth);
		SET_UHE_SETTING(RenderingSettings, RenderHeight);
		SET_UHE_SETTING(RenderingSettings, bTemporalAA);
		SET_UHE_SETTING(RenderingSettings, bEnableRayTracing);
		SET_UHE_SETTING(RenderingSettings, bEnableGPULabeling);
		SET_UHE_SETTING(RenderingSettings, bEnableLayerValidation);
		SET_UHE_SETTING(RenderingSettings, bEnableGPUTiming);
		SET_UHE_SETTING(RenderingSettings, bEnableDepthPrePass);
		SET_UHE_SETTING(RenderingSettings, ParallelSubmitters);
		SET_UHE_SETTING(RenderingSettings, RTCullingRadius);
		SET_UHE_SETTING(RenderingSettings, RTShadowQuality);
		SET_UHE_SETTING(RenderingSettings, RTShadowTMax);
		SET_UHE_SETTING(RenderingSettings, RTReflectionQuality);
		SET_UHE_SETTING(RenderingSettings, RTReflectionTMax);
		SET_UHE_SETTING(RenderingSettings, RTReflectionSmoothCutoff);
		SET_UHE_SETTING(RenderingSettings, FinalReflectionStrength);
		SET_UHE_SETTING(RenderingSettings, bDenoiseRayTracing);
		SET_UHE_SETTING(RenderingSettings, bEnableAsyncCompute);
		SET_UHE_SETTING(RenderingSettings, bEnableHDR);
		SET_UHE_SETTING(RenderingSettings, OcclusionTriangleThreshold);
		SET_UHE_SETTING(RenderingSettings, HDRWhitePaperNits);
		SET_UHE_SETTING(RenderingSettings, HDRContrast);
		SET_UHE_SETTING(RenderingSettings, GammaCorrection);
		SET_UHE_SETTING(RenderingSettings, PCSSKernal);
		SET_UHE_SETTING(RenderingSettings, PCSSMinPenumbra);
		SET_UHE_SETTING(RenderingSettings, PCSSMaxPenumbra);
		SET_UHE_SETTING(RenderingSettings, PCSSBlockerDistScale);
		SET_UHE_SETTING(RenderingSettings, bEnableHardwareOcclusion);
		SET_UHE_SETTING(RenderingSettings, SelectedGpuName);
		SET_UHE_SETTING(RenderingSettings, bEnableRTShadow);
		SET_UHE_SETTING(RenderingSettings, bEnableRTReflection);
		SET_UHE_SETTING(RenderingSettings, bEnableRTIndirectLighting);
		SET_UHE_SETTING(RenderingSettings, RTIndirectLightScale);
		SET_UHE_SETTING(RenderingSettings, RTIndirectLightFadeDistance);
		SET_UHE_SETTING(RenderingSettings, RTIndirectTMax);
		SET_UHE_SETTING(RenderingSettings, RTIndirectOcclusionDistance);
	}
}

// save config
void UHConfigManager::SaveConfig(HWND InWindow)
{
	ApplyConfig();

	std::ofstream FileOut(L"UHESettings.ini", std::ios::out);
	if (FileOut.is_open())
	{
		for (size_t SectionIdx = 0; SectionIdx < UHESettings.size(); SectionIdx++)
		{
			UHUtilities::WriteINISection(FileOut, UHESettings[SectionIdx].SectionName);
			for (size_t KeyValueIdx = 0; KeyValueIdx < UHESettings[SectionIdx].KeyValue.size(); KeyValueIdx++)
			{
				const UHIniKeyValue& KeyValue = UHESettings[SectionIdx].KeyValue[KeyValueIdx];
				UHUtilities::WriteINIData(FileOut, KeyValue.KeyName, KeyValue.ValueName);
			}

			FileOut << std::endl;
		}
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