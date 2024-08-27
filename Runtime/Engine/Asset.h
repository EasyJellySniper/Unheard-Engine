#pragma once
#include "../Classes/Mesh.h"
#include "../Classes/Material.h"
#include "../Classes/Texture2D.h"
#include "../Classes/TextureCube.h"

#if WITH_EDITOR
#include "../../Editor/Classes/ShaderImporter.h"
#include "../../Editor/Classes/MaterialImporter.h"
#endif

class UHGraphic;

// asset manager class in UH
class UHAssetManager
{
public:
	UHAssetManager();
	void Release();
	void ImportMeshes();

	void TranslateHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName, UHMaterialCompileData InData
		, std::vector<std::string> Defines, std::filesystem::path& OutputShaderPath);
	void CompileShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
		, std::vector<std::string> Defines);

	void ImportTextures(UHGraphic* InGfx);
	void ImportCubemaps(UHGraphic* InGfx);
	void ImportMaterials(UHGraphic* InGfx);
	void MapTextureIndex(UHMaterial* InMat);

	const std::vector<UHMesh*>& GetUHMeshes() const;
	const std::vector<UHMaterial*>& GetMaterials() const;
	const std::vector<UHTexture2D*>& GetTexture2Ds() const;
	const std::vector<UHTexture2D*>& GetReferencedTexture2Ds() const;
	const std::vector<UHTextureCube*>& GetCubemaps() const;

	UHTexture2D* GetTexture2D(std::string InName) const;
	UHTexture2D* GetTexture2DByPath(std::filesystem::path InPath) const;
	UHTextureCube* GetCubemapByName(std::string InName) const;
	UHTextureCube* GetCubemapByPath(std::filesystem::path InPath) const;

	UHMaterial* GetMaterial(std::string InName) const;
	UHMesh* GetMesh(std::string InName) const;

	// general function for getting an asset, caller is responsible for type cast
	UHObject* GetAsset(UUID InAssetUuid) const;
	void AddImportedMaterial(UHGraphic* InGfx, std::filesystem::path InPath);

#if WITH_EDITOR
	void AddTexture2D(UHTexture2D* InTexture2D);
	void AddImportedMesh(UniquePtr<UHMesh>& InMesh);

	static UHAssetManager* GetAssetMgrEditor();
	static UHTexture2D* GetTexture2DByPathEditor(std::string InName);
	static std::string FindTexturePathName(std::string InName);

	void AddCubemap(UHTextureCube* InCube);
	void RemoveCubemap(UHTextureCube* InCube);
#endif

private:
	void RemoveFromAssetList(UHObject* InObj);

	// loaded meshes
	std::vector<UniquePtr<UHMesh>> UHMeshes;

	// cache of loaded meshes
	std::vector<UHMesh*> UHMeshesCache;

#if WITH_EDITOR
	// shader importer, only compile shader in debug mode
	UniquePtr<UHShaderImporter> UHShaderImporterInterface;

	// material importer
	UniquePtr<UHMaterialImporter> UHMaterialImporterInterface;

	static UHAssetManager* AssetMgrEditorOnly;
#endif

	// loaded & cached UH materials
	std::vector<UHMaterial*> UHMaterialsCache;

	// loaded textures
	std::vector<UHTexture2D*> UHTexture2Ds;
	std::vector<UHTexture2D*> ReferencedTexture2Ds;
	std::vector<UHTextureCube*> UHCubemaps;

	// general list for looking up
	std::vector<UHObject*> AllAssets;
};