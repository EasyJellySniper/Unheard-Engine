#pragma once
#define NOMINMAX
#include "../UnheardEngine.h"

#if WITH_EDITOR
#include <memory>
#include <fbxsdk.h>
#include <filesystem>	// for list all files under a directory and subdirectory, C++17 feature
#include "../Runtime/Classes/Mesh.h"
#include "../Runtime/Classes/Material.h"

class UHFbxImporter
{
public:
	UHFbxImporter();
	~UHFbxImporter();

	// this will output the UHMesh list
	void ImportRawFbx(std::filesystem::path InPath
		, std::filesystem::path InTextureRefPath
		, std::vector<UniquePtr<UHMesh>>& ImportedMesh
		, std::vector<UniquePtr<UHMaterial>>& ImportedMaterial);

private:
	// create UH meshes
	void CreateUHMeshes(FbxNode* InNode, std::filesystem::path InPath
		, std::filesystem::path InTextureRefPath
		, std::vector<UniquePtr<UHMesh>>& ImportedMesh
		, std::vector<UniquePtr<UHMaterial>>& ImportedMaterial);

	FbxManager* FbxSDKManager;
	std::vector<std::string> ImportedMaterialNames;
};

#endif