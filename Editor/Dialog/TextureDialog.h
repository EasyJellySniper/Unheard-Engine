#pragma once
#include "Dialog.h"

#if WITH_DEBUG
#include <unordered_map>
#include "TextureCreationDialog.h"
#include <shtypes.h>

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

	// control functions
	void ControlApply();
	void ControlSave();
	void ControlSaveAll();
	void ControlTextureCreate();
	void ControlBrowseRawTexture();

	UHAssetManager* AssetMgr;
	UHTextureImporter TextureImporter;
	UHGraphic* Gfx;
	UHDeferredShadingRenderer* Renderer;
	int32_t CurrentTextureIndex;
	UHTexture2D* CurrentTexture;
	uint32_t CurrentMip;

	// declare function pointer type for editor control
	std::unordered_map<int32_t, void(UHTextureDialog::*)()> ControlCallbacks;
	std::unique_ptr<UHPreviewScene> PreviewScene;
};

#endif