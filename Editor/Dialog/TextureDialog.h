#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include "TextureCreationDialog.h"
#include <shtypes.h>

class UHAssetManager;
class UHTexture2D;
class UHPreviewScene;
class UHGraphic;
class UHDeferredShadingRenderer;

// compression mode text, need to follow the enum order defined in Texture.h
const std::vector<std::wstring> GCompressionModeText = { L"None", L"BC1 (RGB)", L"BC3 (RGBA)", L"BC4 (R)", L"BC5 (RG)" };
const COMDLG_FILTERSPEC GImageFilter = { {L"Image Formats"}, { L"*.jpg;*.jpeg;*.png;*.bmp"} };

class UHTextureDialog : public UHDialog
{
public:
	UHTextureDialog(UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer);

	virtual void ShowDialog() override;
	void Init();
	void Update();
	void OnCreationFinished(UHTexture* InTexture);

private:
	void SelectTexture(int32_t TexIndex);

	// control functions
	void ControlApply();
	void ControlSave();
	void ControlSaveAll();
	void ControlTextureCreate();
	void ControlBrowseRawTexture();
	void RefreshImGuiMipLevel();

	UHAssetManager* AssetMgr;
	UHTextureImporter TextureImporter;
	UHGraphic* Gfx;
	UHDeferredShadingRenderer* Renderer;
	int32_t CurrentTextureIndex;
	UHTexture2D* CurrentTexture;
	int32_t CurrentMip;
	UHTextureSettings CurrentEditingSettings;
	VkDescriptorSet CurrentTextureDS;

	UniquePtr<UHTextureCreationDialog> TextureCreationDialog;
};

#endif