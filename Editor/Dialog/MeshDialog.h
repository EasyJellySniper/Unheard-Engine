#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../Editor/PreviewScene.h"

class UHAssetManager;
class UHGraphic;

class UHMeshDialog : public UHDialog
{
public:
	UHMeshDialog(UHAssetManager* InAsset, UHGraphic* InGfx);
	~UHMeshDialog();

	virtual void ShowDialog() override;
	virtual void Update(bool& bIsDialogActive) override;

private:
	void SelectMesh(UHMesh* InMesh);

	UHAssetManager* AssetMgr;
	int32_t CurrentMeshIndex;

	UniquePtr<UHPreviewScene> PreviewScene;
	VkDescriptorSet CurrentTextureDS;
};

#endif