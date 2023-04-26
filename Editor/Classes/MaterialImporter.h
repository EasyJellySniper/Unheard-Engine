#pragma once
#define NOMINMAX
#include "../UnheardEngine.h"

#if WITH_DEBUG
#include <filesystem>
#include <vector>
#include "../Runtime/Classes/Utility.h"

class UHMaterial;

// shader asset cache
struct UHMaterialAssetCache
{
	UHMaterialAssetCache()
		: SourceLastModifiedTime(0)
		, SpvGeneratedTime(0)
	{

	}

	inline bool operator==(UHMaterialAssetCache InCache)
	{
		return SourcePath == InCache.SourcePath
			&& SourceLastModifiedTime == InCache.SourceLastModifiedTime
			&& SpvGeneratedTime == InCache.SpvGeneratedTime;
	}

	std::filesystem::path SourcePath;
	int64_t SourceLastModifiedTime;
	int64_t SpvGeneratedTime;
};

class UHMaterialImporter
{
public:
	UHMaterialImporter();
	void LoadMaterialCache();
	void WriteMaterialCache(UHMaterial* InMat, std::string InShaderName);
	bool IsMaterialCached(UHMaterial* InMat, std::string InShaderName);

private:
	std::vector<UHMaterialAssetCache> UHMaterialsCache;
};


#endif