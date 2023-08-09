#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>
#include "../Classes/TextureImporter.h"
#include "../Controls/Button.h"
#include "../Controls/CheckBox.h"
#include "../Controls/ComboBox.h"
#include "../Controls/TextBox.h"

class UHTextureDialog;
class UHTextureCreationDialog : public UHDialog
{
public:
	UHTextureCreationDialog();
	UHTextureCreationDialog(HINSTANCE InInstance, HWND InWindow, UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter);

	virtual void ShowDialog() override;

private:
	UniquePtr<UHComboBox> CompressionModeGUI;
	UniquePtr<UHCheckBox> SrgbGUI;
	UniquePtr<UHCheckBox> NormalGUI;
	UniquePtr<UHButton> CreateTextureGUI;
	UniquePtr<UHButton> BrowseInputGUI;
	UniquePtr<UHButton> BrowseOutputGUI;
	UniquePtr<UHTextBox> TextureInputGUI;
	UniquePtr<UHTextBox> TextureOutputGUI;

	void ControlTextureCreate();
	void ControlBrowserInput();
	void ControlBrowserOutputFolder();

	UHTextureImporter* TextureImporter;
	UHTextureDialog* TextureDialog;
};

#endif