#pragma once
#include "../Classes/Mesh.h"
#include "../Classes/Material.h"
#include "../Classes/Texture2D.h"

#if WITH_EDITOR
#include "../../Editor/Classes/FbxImporter.h"
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
	void ImportMaterials(UHGraphic* InGfx);
	void MapTextureIndex(UHMaterial* InMat);

	std::vector<UHMesh*> GetUHMeshes() const;
	std::vector<UHMaterial*> GetMaterials() const;
	std::vector<UHTexture2D*> GetTexture2Ds() const;
	std::vector<UHTexture2D*> GetReferencedTexture2Ds() const;
	UHTexture2D* GetTexture2D(std::string InName) const;
	UHTexture2D* GetTexture2DByPath(std::filesystem::path InPath) const;
	UHMaterial* GetMaterial(std::string InName) const;
	UHMesh* GetMesh(std::string InName) const;

#if WITH_EDITOR
	void AddTexture2D(UHTexture2D* InTexture2D);
	static UHAssetManager* GetAssetMgrEditor();
	static UHTexture2D* GetTexture2DByPathEditor(std::string InName);
	static std::string FindTexturePathName(std::string InName);
#endif

private:
	// loaded meshes
	std::vector<UniquePtr<UHMesh>> UHMeshes;

	// cache of loaded meshes
	std::vector<UHMesh*> UHMeshesCache;

#if WITH_EDITOR
	// fbx importer class, only import raw mesh in debug mode
	UniquePtr<UHFbxImporter> UHFbxImporterInterface;

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
};