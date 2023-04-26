#pragma once
#include "../UnheardEngine.h"

#if WITH_DEBUG

#include "../Runtime/Classes/Utility.h"
#include <string>
#include <vector>
#include <wrl.h>
#include <wincodec.h>
#include <filesystem>
#include "../Runtime/Classes/Texture2D.h"
using namespace Microsoft::WRL;

#define RETURN_IF_FAILED(x) if(x != S_OK) return std::vector<uint8_t>();

struct UHRawTextureAssetCache
{
	UHRawTextureAssetCache()
		: SourcLastModifiedTime(0)
	{

	}

	inline bool operator==(const UHRawTextureAssetCache& InCache)
	{
		// cache is equal if source path, modified time, and UHTexture path are matched
		const bool bIsCacheEqual = (InCache.SourcePath == SourcePath)
			&& (InCache.SourcLastModifiedTime == SourcLastModifiedTime);

		bool bIsTexturePathEqual = UHTexturePath == InCache.UHTexturePath;

		return bIsCacheEqual && bIsTexturePathEqual;
	}

	std::filesystem::path SourcePath;
	int64_t SourcLastModifiedTime;
	std::filesystem::path UHTexturePath;
};

class UHTextureImporter
{
public:
	UHTextureImporter();

	// load texture from file, reference: https://github.com/microsoft/Xbox-ATG-Samples/blob/main/PCSamples/IntroGraphics/SimpleTexturePC12/SimpleTexturePC12.cpp
	std::vector<uint8_t> LoadTexture(std::wstring Filename, uint32_t& Width, uint32_t& Height);

	bool IsUHTextureCached(std::filesystem::path InPath);
	bool IsRawTextureCached(std::filesystem::path InPath);
	void AddRawTextureAssetCache(UHRawTextureAssetCache InAssetCache);
	void ImportRawTextures();

	// load caches
	void LoadCaches();

	// output caches
	void OutputCaches();

private:
	std::vector<UHRawTextureAssetCache> TextureAssetCaches;

	// @TODO: Better gamma/linear textures setting, get rid of this after I have an editor
	std::vector<std::string> GammaTextures;
};
#endif