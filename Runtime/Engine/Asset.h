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

struct UHAssetMap
{
	UHAssetMap() 
		: Asset(nullptr)
		, AssetUUid(UUID())
	{
	}

	UHAssetMap(UHObject* InObj, std::string InPath)
		: AssetUUid(InObj->GetRuntimeGuid())
		, FilePath(InPath)
		, Asset(InObj)
	{

	}

	UUID AssetUUid;
	std::string FilePath;
	UHObject* Asset;
};

// asset manager class in UH
class UHAssetManager
{
public:
	UHAssetManager();
	void SetGfxCache(UHGraphic* InGfx);
	void ImportBuiltInAssets();
	void Release();

	void TranslateHLSL(UHShader* InShader, UHMaterialCompileData InData, std::filesystem::path& OutputShaderPath);
	void CompileShader(UHShader* InShader);

	UHObject* ImportAsset(std::filesystem::path InPath);
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
	UHObject* GetAsset(UUID InAssetUuid);
	UHObject* GetAsset(std::string InPath);
	UHObject* AddImportedMaterial(std::filesystem::path InPath);

#if WITH_EDITOR
	void ImportAssets();
	void AddTexture2D(UHTexture2D* InTexture2D);
	void AddImportedMesh(UniquePtr<UHMesh>& InMesh);

	static UHAssetManager* GetAssetMgrEditor();
	static UHTexture2D* GetTexture2DByPathEditor(std::string InName);
	static std::string FindTexturePathName(std::string InName);

	void AddCubemap(UHTextureCube* InCube);
	UHShaderImporter* GetShaderImporter() const;
#endif

private:
	void ClearAssetCaches();
	UHObject* ImportMesh(std::filesystem::path InPath);
	UHObject* ImportTexture(std::filesystem::path InPath);
	UHObject* ImportCubemap(std::filesystem::path InPath);
	UHObject* ImportMaterial(std::filesystem::path InPath);

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

	UHGraphic* GfxCache;

	// loaded & cached UH materials
	std::vector<UHMaterial*> UHMaterialsCache;

	// loaded textures
	std::vector<UHTexture2D*> UHTexture2Ds;
	std::vector<UHTexture2D*> ReferencedTexture2Ds;
	std::vector<UHTextureCube*> UHCubemaps;

	// general list for looking up
	std::vector<UHAssetMap> AllAssetsMap;
};