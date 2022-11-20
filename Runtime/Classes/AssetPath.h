#pragma once
#include <string>
#include "Utility.h"

// texture paths
static std::string GTextureAssetFolder = "Assets/Textures/";
static std::string GRawTexturePath = "RawAssets/Textures/";
static std::string GTextureAssetCachePath = "AssetCaches/Textures/";
static std::string GTextureAssetExtension = ".uhtexture";
static std::string GTextureAssetCacheExtension = ".uhtexturecache";

// mesh paths
static std::string GMeshAssetFolder = "Assets/Meshes/";
static std::string GRawMeshAssetPath = "RawAssets/Meshes/";
static std::string GMeshAssetCachePath = "AssetCaches/Meshes/";
static std::string GBuiltInMeshAssetPath = "Assets/Meshes/BuiltIn/";
static std::string GMeshAssetExtension = ".uhmesh";
static std::string GMeshAssetCacheExtension = ".uhmeshcache";
static std::string GRawMeshAssetExtension = ".fbx";

// shader paths
static std::string GRawShaderCachePath = "AssetCaches/Shaders/";
static std::string GRawShaderPath = "Shaders/";
static std::string GShaderAssetFolder = "Assets/Shaders/";
static std::string GShaderAssetExtension = ".spv";
static std::string GShaderAssetCacheExtension = ".uhshadercache";

// material paths
static std::string GMaterialAssetPath = "Assets/Materials/";
static std::string GMaterialAssetExtension = ".uhmaterial";

namespace UHAssetPath
{
	inline std::string GetShaderOriginSubpath(std::filesystem::path InSource)
	{
		std::filesystem::path OriginPath = InSource;
		OriginPath = OriginPath.remove_filename();
		std::string OriginSubpath = UHUtilities::RemoveSubString(OriginPath.string(), GRawShaderPath);

		return OriginSubpath;
	}

	inline std::string GetMeshOriginSubpath(std::filesystem::path InSource)
	{
		std::filesystem::path OriginPath = InSource;
		OriginPath = OriginPath.remove_filename();
		std::string OriginSubpath = UHUtilities::RemoveSubString(OriginPath.string(), GRawMeshAssetPath);

		return OriginSubpath;
	}

	inline std::string GetTextureOriginSubpath(std::filesystem::path InSource)
	{
		std::filesystem::path OriginPath = InSource;
		OriginPath = OriginPath.remove_filename();
		std::string OriginSubpath = UHUtilities::RemoveSubString(OriginPath.string(), GRawTexturePath);

		return OriginSubpath;
	}

	inline bool IsTheSameExtension(std::filesystem::path InSource, std::string InExt)
	{
		std::string SrcExt = UHUtilities::ToLowerString(InSource.extension().string());
		std::string DstExt = UHUtilities::ToLowerString(InExt);

		return SrcExt == DstExt;
	}
}