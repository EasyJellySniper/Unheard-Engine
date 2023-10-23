#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include "../Classes/TextureImporter.h"

class UHTextureDialog;
class UHTextureCreationDialog : public UHDialog
{
public:
	UHTextureCreationDialog(HWND InWindow, UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter);
	virtual void Update() override;

private:
	void ControlTextureCreate();
	void ControlBrowserInput();
	void ControlBrowserOutputFolder();

	UHTextureImporter* TextureImporter;
	UHTextureDialog* TextureDialog;
	UHTextureSettings CurrentEditingSettings;
	std::string CurrentSourceFile;
	std::string CurrentOutputPath;
};

#endif