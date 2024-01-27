#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include "../Editor/PreviewScene.h"

class UHAssetManager;
class UHGraphic;
class UHDeferredShadingRenderer;
class UHFbxImporter;

class UHMeshDialog : public UHDialog
{
public:
	UHMeshDialog(UHAssetManager* InAsset, UHGraphic* InGfx, UHDeferredShadingRenderer* InRenderer, UHRawInput* InInput);
	~UHMeshDialog();

	virtual void ShowDialog() override;
	virtual void Update(bool& bIsDialogActive) override;

private:
	void SelectMesh(UHMesh* InMesh);
	void OnImport();

	UHAssetManager* AssetMgr;
	UHGraphic* Gfx;
	UHDeferredShadingRenderer* Renderer;
	UHRawInput* Input;
	int32_t CurrentMeshIndex;

	UniquePtr<UHPreviewScene> PreviewScene;
	// fbx importer class, only import raw mesh in debug mode
	UniquePtr<UHFbxImporter> FBXImporterInterface;
	VkDescriptorSet CurrentTextureDS;

	std::string InputSourceFile;
	std::string MeshOutputPath;
	std::string MaterialOutputPath;
	std::string TextureReferencePath;

	bool bCreateRendererAfterImport;
};

#endif