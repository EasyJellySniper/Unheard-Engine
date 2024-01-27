#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include "TextureDialog.h"
#include <shtypes.h>

class UHAssetManager;
class UHTextureCube;
class UHGraphic;
class UHDeferredShadingRenderer;

class UHCubemapDialog : public UHDialog
{
public:
	UHCubemapDialog(UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer);

	virtual void ShowDialog() override;
	void Init();
	virtual void Update(bool& bIsDialogActive) override;
	void OnCreationFinished(UHTexture* InTexture);

private:
	void SelectCubemap(int32_t TexIndex);

	// control functions
	void ControlSave();
	void ControlSaveAll();
	void RefreshImGuiMipLevel();

	UHAssetManager* AssetMgr;
	UHGraphic* Gfx;
	UHDeferredShadingRenderer* Renderer;

	int32_t CurrentCubeIndex;
	UHTextureCube* CurrentCube;
	int32_t CurrentMip;
	UHTextureSettings CurrentEditingSettings;
	VkDescriptorSet CurrentCubeDS[6];

	UniquePtr<UHTextureCreationDialog> TextureCreationDialog;
	std::vector<VkDescriptorSet> CubeDSToRemove;
};

#endif