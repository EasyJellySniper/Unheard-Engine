#pragma once
#include "Dialog.h"

#if WITH_EDITOR
class UHFbxImporter;
class UHEngine;

class UHFbxImportDialog : public UHDialog
{
public:
	UHFbxImportDialog(UHEngine* InEngine);
	~UHFbxImportDialog();

	virtual void ShowDialog() override;
	virtual void Update(bool& bIsDialogActive) override;

private:
	void OnImport();

	UHEngine* Engine;
	UniquePtr<UHFbxImporter> FBXImporterInterface;

	std::string InputSourceFile;
	std::string MeshOutputPath;
	std::string MaterialOutputPath;
	std::string TextureReferencePath;
	std::string CurrConvertUnitText;

	bool bCreateSceneObjectAfterImport;
	bool bCreateNewScene;
};

#endif