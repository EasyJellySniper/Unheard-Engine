#pragma once
#define NOMINMAX
#include "../UnheardEngine.h"

#if WITH_DEBUG
#include <memory>
#include <fbxsdk.h>
#include <filesystem>	// for list all files under a directory and subdirectory, C++17 feature
#include "../Runtime/Classes/Mesh.h"
#include "../Runtime/Classes/Material.h"

struct UHRawMeshAssetCache
{
	UHRawMeshAssetCache()
		: SourcLastModifiedTime(0)
	{

	}

	inline bool operator==(const UHRawMeshAssetCache& InCache)
	{
		// cache is equal if source path, modified time, and UHMeshes path are matched
		const bool bIsCacheEqual = (InCache.SourcePath == SourcePath)
			&& (InCache.SourcLastModifiedTime == SourcLastModifiedTime);

		bool bIsMeshPathEqual = true;
		if (InCache.UHMeshesPath.size() == UHMeshesPath.size())
		{
			for (size_t Idx = 0; Idx < UHMeshesPath.size(); Idx++)
			{
				if (InCache.UHMeshesPath[Idx] != UHMeshesPath[Idx])
				{
					bIsMeshPathEqual = false;
				}
			}
		}
		else
		{
			bIsMeshPathEqual = false;
		}

		return bIsCacheEqual && bIsMeshPathEqual;
	}

	std::filesystem::path SourcePath;
	int64_t SourcLastModifiedTime;
	std::vector<std::filesystem::path> UHMeshesPath;
};

class UHFbxImporter
{
public:
	UHFbxImporter();
	~UHFbxImporter();

	// this will output the UHMesh list
	void ImportRawFbx(std::vector<UniquePtr<UHMaterial>>& InMatVector);

	// is UHMesh cached?
	bool IsUHMeshCached(std::filesystem::path InUHMeshAssetPath);

private:
	// create UH meshes
	void CreateUHMeshes(FbxNode* InNode, std::filesystem::path InPath, std::vector<UniquePtr<UHMaterial>>& InMatVector, UHRawMeshAssetCache& Cache);

	// add raw mesh asset cache
	void AddRawMeshAssetCache(UHRawMeshAssetCache InAssetCache);

	// load caches
	void LoadCaches();

	// output caches
	void OutputCaches();

	// check if a raw asset is cached
	bool IsCached(std::filesystem::path InRawAssetPath);

	FbxManager* FbxSDKManager;
	std::vector<UHRawMeshAssetCache> MeshAssetCaches;
	std::vector<std::string> ImportedMaterialNames;
};

#endif