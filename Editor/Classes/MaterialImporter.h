#pragma once
#define NOMINMAX
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include <filesystem>
#include <vector>
#include "Runtime/Classes/Utility.h"

class UHMaterial;
class UHShader;

// shader asset cache
struct UHMaterialAssetCache
{
	UHMaterialAssetCache()
		: SpvGeneratedTime(0)
		, MacroHash(0)
		, SourceModifiedTime(0)
	{

	}

	inline bool operator==(UHMaterialAssetCache InCache)
	{
		return SourcePath == InCache.SourcePath
			&& SpvGeneratedTime == InCache.SpvGeneratedTime
			&& MacroHash == InCache.MacroHash
			&& SourceModifiedTime == InCache.SourceModifiedTime;
	}

	std::filesystem::path SourcePath;
	int64_t SpvGeneratedTime;
	size_t MacroHash;
	int64_t SourceModifiedTime;
};

class UHMaterialImporter
{
public:
	UHMaterialImporter();
	void LoadMaterialCache();
	void WriteMaterialCache(UHMaterial* InMat, UHShader* InShader);
	bool IsMaterialCached(UHMaterial* InMat, UHShader* InShader);

private:
	std::vector<UHMaterialAssetCache> UHMaterialsCache;
};


#endif