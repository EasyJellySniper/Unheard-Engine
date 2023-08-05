#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>
#include "../Classes/TextureImporter.h"

class UHTextureDialog;
class UHTextureCreationDialog : public UHDialog
{
public:
	UHTextureCreationDialog();
	UHTextureCreationDialog(HINSTANCE InInstance, HWND InWindow, UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter);

	virtual void ShowDialog() override;
	void Update();

	void TerminateDialog();

private:
	void ControlTextureCreate();
	void ControlBrowserInput();
	void ControlBrowserOutputFolder();

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHTextureCreationDialog::*)()> ControlCallbacks;

	UHTextureImporter* TextureImporter;
	UHTextureDialog* TextureDialog;
};

#endif