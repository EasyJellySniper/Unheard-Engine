#include "Application.h"
#include "Platform/Client.h"
#include <cstdlib>
#include <ctime>
#include "Editor/Dialog/StatusDialog.h"

UHApplication::UHApplication()
	: Platform(nullptr)
	, Engine(nullptr)
{

}

UHEngine* UHApplication::GetEngine()
{
	return Engine.get();
}

int32_t UHApplication::Run()
{
	// setup rand seed
	srand((uint32_t)time(nullptr));

	// platform specific initialization
	Platform = MakeUnique<UHPlatform>();
	if (!Platform->Initialize(this))
	{
		Platform->Shutdown();
		return 0;
	}

	// engine initialization
	{
		UHStatusDialogScope StatusDialog("Loading...");
		Engine = MakeUnique<UHEngine>();
		Engine->LoadConfig();

		UHClient* Client = Platform->GetClient();
		if (!Engine->InitEngine(Client))
		{
			UHE_LOG(L"Engine creation failed!\n");
			Engine->ReleaseEngine();
			Engine.reset();
			return 0;
		}
	}

	// game engine loop
	UHClient* Client = Platform->GetClient();
	while (true)
	{
		// process client events
		Client->ProcessEvents();
		if (Client->IsQuit())
		{
			break;
		}

		// call the game loop
		if (Engine)
		{
			Engine->BeginFPSLimiter();
#if WITH_EDITOR
			Engine->BeginProfile();
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// profile does not contain editor update time
			Engine->GetEditor()->OnEditorUpdate();
#endif

			// update anyway, could consider adding a setting to decide wheter to pause update when it's minimized
			Engine->Update();

#if WITH_EDITOR
			ImGui::Render();
#endif

			// only call render loop when it's not minimized
			if (!Client->IsWindowMinimized())
			{
				Engine->RenderLoop();
			}

#if WITH_EDITOR               
			// tricky workaround for HDR toggling, when the platform window of ImGui is outside the main window
			// the window needs to be re-created to match the swapchain format
			static bool bIsHDRAvailablePrev = Engine->GetGfx()->IsHDRAvailable();
			ImGui::UpdatePlatformWindows(bIsHDRAvailablePrev != Engine->GetGfx()->IsHDRAvailable());
			bIsHDRAvailablePrev = Engine->GetGfx()->IsHDRAvailable();

			// assume multi-view is always enabled
			ImGui_Vulkan_CustomData CustomData{};
			CustomData.Pipeline = Engine->GetGfx()->GetImGuiPipeline();
			ImGui::RenderPlatformWindowsDefault(nullptr, &CustomData);
#endif

			Engine->EndFPSLimiter();
#if WITH_EDITOR
			Engine->EndProfile();
#endif
		}
	}

	// save config and release engine when destroy
	if (Engine)
	{
		Engine->SaveConfig();
		Engine->ReleaseEngine();
		Engine.reset();
	}

	Platform->Shutdown();
	return 0;
}