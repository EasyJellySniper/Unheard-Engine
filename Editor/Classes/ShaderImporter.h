#pragma once
#include "../UnheardEngine.h"

#if WITH_EDITOR
#include <filesystem>
#include <vector>
#include "../Runtime/Classes/Utility.h"

class UHMaterial;
struct UHMaterialCompileData;

// shader asset cache
struct UHRawShaderAssetCache
{
	UHRawShaderAssetCache()
		: SourcLastModifiedTime(0)
	{

	}

	inline bool operator==(UHRawShaderAssetCache InCache)
	{
		bool bDefineEqual = Defines.size() == InCache.Defines.size();
		if (bDefineEqual)
		{
			for (size_t Idx = 0; Idx < Defines.size(); Idx++)
			{
				bDefineEqual &= Defines[Idx] == InCache.Defines[Idx];
			}
		}

		// cache is equal if source path, modified time, and UHMeshes path are matched
		const bool bIsCacheEqual = (InCache.SourcePath == SourcePath)
			&& (InCache.SourcLastModifiedTime == SourcLastModifiedTime)
			&& (InCache.UHShaderPath == UHShaderPath)
			&& (InCache.EntryName == EntryName)
			&& (InCache.ProfileName == ProfileName);

		return bIsCacheEqual && bDefineEqual;
	}

	std::filesystem::path SourcePath;
	int64_t SourcLastModifiedTime;
	std::filesystem::path UHShaderPath;
	std::string EntryName;
	std::string ProfileName;
	std::vector<std::string> Defines;
};

class UHShaderImporter
{
public:
	UHShaderImporter();

	void LoadShaderCache();
	void WriteShaderIncludeCache();
	bool IsShaderIncludeCached();
	bool IsShaderCached(std::filesystem::path SourcePath, std::filesystem::path UHShaderPath, std::string EntryName, std::string ProfileName
		, std::vector<std::string> Defines);
	bool IsShaderTemplateCached(std::filesystem::path SourcePath, std::string EntryName, std::string ProfileName);
	void CompileHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
		, std::vector<std::string> Defines);
	std::string TranslateHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName, UHMaterialCompileData InData
		, std::vector<std::string> Defines);

private:
	std::vector<UHRawShaderAssetCache> UHRawShadersCache;
	std::vector<std::string> ShaderIncludes;
};

#endif