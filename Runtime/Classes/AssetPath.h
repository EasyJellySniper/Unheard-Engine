#pragma once
#include <string>
#include "Utility.h"

// general paths
static std::string GTempFilePath = "Temp/";

// texture paths
static std::string GTextureAssetFolder = "Assets/Textures/";
static std::string GTextureAssetExtension = ".uhtexture";

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
static std::string GRawShaderExtension = ".hlsl";
static std::string GShaderAssetFolder = "Assets/Shaders/";
static std::string GShaderAssetExtension = ".spv";
static std::string GShaderAssetCacheExtension = ".uhshadercache";

// material paths
static std::string GMaterialAssetPath = "Assets/Materials/";
static std::string GMaterialAssetExtension = ".uhmaterial";
static std::string GMaterialCachePath = "AssetCaches/Materials/";
static std::string GMaterialCacheExtension = ".uhmaterialcache";

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

	inline std::string GetMaterialOriginSubpath(std::filesystem::path InSource)
	{
		std::filesystem::path OriginPath = InSource;
		OriginPath = OriginPath.remove_filename();
		std::string OriginSubpath = UHUtilities::RemoveSubString(OriginPath.string(), GMaterialAssetPath);

		return OriginSubpath;
	}

	inline bool IsTheSameExtension(std::filesystem::path InSource, std::string InExt)
	{
		std::string SrcExt = UHUtilities::ToLowerString(InSource.extension().string());
		std::string DstExt = UHUtilities::ToLowerString(InExt);

		return SrcExt == DstExt;
	}

	inline std::string FormatMaterialShaderOutputPath(std::string OriginSubpath, std::string MaterialName, std::string ShaderName, std::string MacroHash)
	{
		return OriginSubpath + MaterialName + "_" + ShaderName + MacroHash;
	}
}