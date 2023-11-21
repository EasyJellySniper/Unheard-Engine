#include "MaterialImporter.h"

#if WITH_EDITOR
#include "../../Runtime/Classes/AssetPath.h"
#include "../../Runtime/Classes/Utility.h"
#include "../../Runtime/Classes/Material.h"

UHMaterialImporter::UHMaterialImporter()
{
}

void UHMaterialImporter::LoadMaterialCache()
{
	if (!std::filesystem::exists(GMaterialCachePath))
	{
		std::filesystem::create_directories(GMaterialCachePath);
	}

	for (std::filesystem::recursive_directory_iterator Idx(GMaterialCachePath.c_str()), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()) || !UHAssetPath::IsTheSameExtension(Idx->path(), GMaterialCacheExtension))
		{
			continue;
		}

		std::ifstream FileIn(Idx->path().string().c_str(), std::ios::in | std::ios::binary);

		// load cache
		UHMaterialAssetCache Cache;

		// load source path
		std::string TempString;
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.SourcePath = TempString;

		// load last modified time of source
		FileIn.read(reinterpret_cast<char*>(&Cache.SpvGeneratedTime), sizeof(Cache.SpvGeneratedTime));
		FileIn.read(reinterpret_cast<char*>(&Cache.MacroHash), sizeof(Cache.MacroHash));
		FileIn.read(reinterpret_cast<char*>(&Cache.SourceModifiedTime), sizeof(Cache.SourceModifiedTime));

		FileIn.close();

		UHMaterialsCache.push_back(Cache);
	}
}

void UHMaterialImporter::WriteMaterialCache(UHMaterial* InMat, std::string InShaderName, std::vector<std::string> Defines)
{
	// create the proper path
	std::string OriginSubpath = UHAssetPath::GetMaterialOriginSubpath(InMat->GetPath());
	if (!std::filesystem::exists(GMaterialCachePath + OriginSubpath))
	{
		std::filesystem::create_directories(GMaterialCachePath + OriginSubpath);
	}

	// macro hash
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(Defines);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";
	std::string OutName = UHAssetPath::FormatMaterialShaderOutputPath("", InMat->GetSourcePath(), InShaderName, MacroHashName);

	std::ofstream FileOut(GMaterialCachePath + OutName + GMaterialCacheExtension, std::ios::out | std::ios::binary);

	// get last modified time and spv generated shader time
	std::string OutputShaderPath = GShaderAssetFolder + OutName + GShaderAssetExtension;
	int64_t SpvGeneratedTime = std::filesystem::exists(OutputShaderPath) ? std::filesystem::last_write_time(OutputShaderPath).time_since_epoch().count() : 0;
	int64_t SourceModifiedTime = std::filesystem::last_write_time(InMat->GetPath()).time_since_epoch().count();

	UHUtilities::WriteStringData(FileOut, InMat->GetPath().string());
	FileOut.write(reinterpret_cast<const char*>(&SpvGeneratedTime), sizeof(SpvGeneratedTime));
	FileOut.write(reinterpret_cast<const char*>(&MacroHash), sizeof(MacroHash));
	FileOut.write(reinterpret_cast<const char*>(&SourceModifiedTime), sizeof(SourceModifiedTime));

	FileOut.close();
}

bool UHMaterialImporter::IsMaterialCached(UHMaterial* InMat, std::string InShaderName, std::vector<std::string> Defines)
{
	if (!std::filesystem::exists(InMat->GetPath()))
	{
		return false;
	}

	// macro hash
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(Defines);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";
	const std::string OriginSubpath = UHAssetPath::GetMaterialOriginSubpath(InMat->GetPath());
	std::string OutName = UHAssetPath::FormatMaterialShaderOutputPath("", InMat->GetSourcePath(), InShaderName, MacroHashName);

	UHMaterialAssetCache Cache;
	Cache.SourcePath = InMat->GetPath();
	Cache.MacroHash = MacroHash;

	std::string OutputShaderPath = GShaderAssetFolder + OutName + GShaderAssetExtension;
	if (!std::filesystem::exists(OutputShaderPath))
	{
		return false;
	}
	Cache.SpvGeneratedTime = std::filesystem::last_write_time(OutputShaderPath).time_since_epoch().count();
	Cache.SourceModifiedTime = std::filesystem::last_write_time(InMat->GetPath()).time_since_epoch().count();

	return UHUtilities::FindByElement(UHMaterialsCache, Cache);
}

#endif