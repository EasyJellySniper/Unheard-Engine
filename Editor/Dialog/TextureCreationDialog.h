#pragma once
#include "Dialog.h"

#if WITH_EDITOR
#include <unordered_map>
#include "../Classes/TextureImporter.h"

class UHTextureDialog;
class UHCubemapDialog;
class UHAssetManager;
class UHGraphic;

class UHTextureCreationDialog : public UHDialog
{
public:
	UHTextureCreationDialog(UHGraphic* InGfx, UHTextureDialog* InTextureDialog, UHTextureImporter* InTextureImporter);
	UHTextureCreationDialog(UHGraphic* InGfx, UHCubemapDialog* InCubemapDialog, UHAssetManager* InAssetMgr);

	void Update();
	void CheckPendingTextureCreation();
	void CheckPendingCubeCreation();

private:
	void ShowTexture2DCreationUI();
	void ShowCubemapCreationUI();

	void ControlTextureCreate();
	void ControlCubemapCreate();
	void ControlBrowserInput();
	void ControlBrowserOutputFolder();

	UHAssetManager* AssetMgr;
	UHGraphic* Gfx;
	UHTextureImporter* TextureImporter;
	UHTextureDialog* TextureDialog;
	UHCubemapDialog* CubemapDialog;
	UHTextureSettings CurrentEditingSettings;

	std::string CurrentSourceFile;
	std::string CurrentOutputPath;
	std::string CurrentSelectedSlice[6];
	std::string CurrentSelectedPanorama;

	bool bNeedCreatingTexture;
	bool bNeedCreatingCube;
	bool bCreatingCubeFromPanorama;
};

#endif