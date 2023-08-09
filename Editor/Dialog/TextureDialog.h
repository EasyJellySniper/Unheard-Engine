#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>
#include "TextureCreationDialog.h"
#include <shtypes.h>
#include "../Controls/Label.h"
#include "../Controls/ListBox.h"
#include "../Controls/Button.h"
#include "../Controls/ComboBox.h"
#include "../Controls/GroupBox.h"
#include "../Controls/CheckBox.h"
#include "../Controls/TextBox.h"
#include "../Controls/Slider.h"

class UHAssetManager;
class UHTexture2D;
class UHPreviewScene;
class UHGraphic;
class UHDeferredShadingRenderer;

// compression mode text, need to follow the enum order defined in Texture.h
const std::vector<std::wstring> GCompressionModeText = { L"None", L"BC1 (RGB)", L"BC3 (RGBA)" };
const COMDLG_FILTERSPEC GImageFilter = { {L"Image Formats"}, { L"*.jpg;*.jpeg;*.png;*.bmp"} };

class UHTextureDialog : public UHDialog
{
public:
	UHTextureDialog(HINSTANCE InInstance, HWND InWindow, UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer);
	~UHTextureDialog();

	virtual void ShowDialog() override;
	void Init();
	void Update();
	void OnCreationFinished(UHTexture* InTexture);

private:
	void SelectTexture(int32_t TexIndex);
	void UpdatePreviewScene();

	// GUI controls
	UniquePtr<UHListBox> TextureListGUI;
	UniquePtr<UHButton> CreateButtonGUI;
	UniquePtr<UHButton> SaveButtonGUI;
	UniquePtr<UHButton> SaveAllButtonGUI;
	UniquePtr<UHLabel> SizeLabelGUI;
	UniquePtr<UHLabel> MipLabelGUI;
	UniquePtr<UHSlider> MipSliderGUI;

	UniquePtr<UHGroupBox> TexturePreviewGUI;
	UniquePtr<UHComboBox> CompressionGUI;
	UniquePtr<UHCheckBox> SrgbGUI;
	UniquePtr<UHCheckBox> NormalGUI;
	UniquePtr<UHButton> ApplyGUI;
	UniquePtr<UHTextBox> FileNameGUI;
	UniquePtr<UHButton> BrowseGUI;

	UniquePtr<UHLabel> CompressionTextGUI;
	UniquePtr<UHLabel> SourceFileTextGUI;
	UniquePtr<UHLabel> HintTextGUI;

	// control functions
	void ControlApply();
	void ControlSave();
	void ControlSaveAll();
	void ControlTextureCreate();
	void ControlBrowseRawTexture();
	void ControlTexturePreview();

	UHAssetManager* AssetMgr;
	UHTextureImporter TextureImporter;
	UHGraphic* Gfx;
	UHDeferredShadingRenderer* Renderer;
	int32_t CurrentTextureIndex;
	UHTexture2D* CurrentTexture;
	uint32_t CurrentMip;

	// preview scene
	UniquePtr<UHPreviewScene> PreviewScene;
	UHTextureCreationDialog TextureCreationDialog;

};

#endif