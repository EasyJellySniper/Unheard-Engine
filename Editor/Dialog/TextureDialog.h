#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include "TextureCreationDialog.h"
#include <shtypes.h>

class UHAssetManager;
class UHTexture2D;
class UHGraphic;
class UHDeferredShadingRenderer;

// compression mode text, need to follow the enum order defined in Texture.h
const std::vector<std::wstring> GCompressionModeText = { L"None", L"BC1 (RGB)", L"BC3 (RGBA)", L"BC4 (R)", L"BC5 (RG)", L"BC6H (RGB HDR)" };
const COMDLG_FILTERSPEC GImageFilter = { {L"Image Formats"}, { L"*.jpg;*.jpeg;*.png;*.bmp;*.exr"} };

class UHTextureDialog : public UHDialog
{
public:
	UHTextureDialog(UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer);

	virtual void ShowDialog() override;
	void Init();
	virtual void Update(bool& bIsDialogActive) override;
	void OnCreationFinished(UHTexture* InTexture);

private:
	void SelectTexture(int32_t TexIndex);

	// control functions
	void ControlApply();
	void ControlSave();
	void ControlSaveAll();
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
	std::vector<VkDescriptorSet> TextureDSToRemove;
};

inline void ValidateTextureSetting(UHTextureSettings& InSetting)
{
	if (InSetting.bIsHDR)
	{
		// hdr texture can never be normal mapping for now
		InSetting.bIsNormal = false;

		// force BC6H compression if it's not uncompressed
		InSetting.CompressionSetting = (InSetting.CompressionSetting != CompressionNone) ? BC6H : InSetting.CompressionSetting;
	}
	else if (InSetting.CompressionSetting == BC6H)
	{
		// if it's not a HDR format and still using BC6H, fallback to BC1
		InSetting.CompressionSetting = BC1;
	}

	else if (InSetting.CompressionSetting == BC4 || InSetting.CompressionSetting == BC5)
	{
		// force linear if it's BC4 or BC5
		InSetting.bIsLinear = true;
	}
	else if (InSetting.bIsNormal)
	{
		// force BC5 compression if it's not uncompressed and a normal map
		InSetting.CompressionSetting = (InSetting.CompressionSetting != CompressionNone) ? BC5 : InSetting.CompressionSetting;
	}
}

#endif