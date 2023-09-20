#include "Asset.h"
#include <fstream>
#include "../Classes/Utility.h"
#include "../Classes/Texture2D.h"
#include "Graphic.h"
#include "../Classes/AssetPath.h"

#if WITH_EDITOR
#include "../../Editor/Classes/GeometryUtility.h"
#include <chrono>
UHAssetManager* UHAssetManager::AssetMgrEditorOnly = nullptr;
#endif

UHAssetManager::UHAssetManager()
{
#if WITH_EDITOR
	UHFbxImporterInterface = MakeUnique<UHFbxImporter>();

	// load shader cache after initialization
	UHShaderImporterInterface = MakeUnique<UHShaderImporter>();
	UHShaderImporterInterface->LoadShaderCache();
	UHShaderImporterInterface->CompileHLSL("FallbackPixelShader", GRawShaderPath + "FallbackPixelShader.hlsl", "FallbackPS", "ps_6_0", std::vector<std::string>());

	UHMaterialImporterInterface = MakeUnique<UHMaterialImporter>();
	UHMaterialImporterInterface->LoadMaterialCache();

	// generate built-in meshes
	UHMesh BuiltInCube = UHGeometryHelper::CreateCubeMesh();
	BuiltInCube.Export(GBuiltInMeshAssetPath, false);

	AssetMgrEditorOnly = this;
#endif
}

void UHAssetManager::Release()
{
	// release meshes
	for (auto& Mesh : UHMeshes)
	{
		if (Mesh != nullptr)
		{
			Mesh->ReleaseCPUMeshData();
			Mesh->Release();
			Mesh.reset();
		}
	}

#if WITH_EDITOR
	// write shader include cache when exitng
	UHShaderImporterInterface->WriteShaderIncludeCache();
	UHShaderImporterInterface.reset();
	UHFbxImporterInterface.reset();
	UHMaterialImporterInterface.reset();
	AssetMgrEditorOnly = nullptr;
#endif

	// container cleanup
	UHMeshes.clear();
	UHMeshesCache.clear();
	UHMaterialsCache.clear();
	UHTexture2Ds.clear();
}

void UHAssetManager::ImportMeshes()
{
	// importing from raw asset first
	// it won't duplicate the import if mesh is cached
#if WITH_EDITOR
	// @TODO: remove the code below after fbx import editor is done
	//std::vector<UniquePtr<UHMaterial>> ImportedMat;
	//UHFbxImporterInterface->ImportRawFbx(ImportedMat);

	//// FBX will also import material, so export as UH material right after import
	//for (UniquePtr<UHMaterial>& Mat : ImportedMat)
	//{
	//	Mat->GenerateDefaultMaterialNodes();
	//	Mat->Export();
	//}

	//for (UniquePtr<UHMaterial>& Mat : ImportedMat)
	//{
	//	Mat.reset();
	//}
	//ImportedMat.clear();
#endif

	// import UHMeshes
	if (!std::filesystem::exists(GMeshAssetFolder))
	{
		std::filesystem::create_directories(GMeshAssetFolder);
	}

	for (std::filesystem::recursive_directory_iterator Idx(GMeshAssetFolder.c_str()), end; Idx != end; Idx++)
	{
		// skip directory
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}

#if WITH_EDITOR
		// skip cache if it's from bulit in mesh folder
		size_t Found = Idx->path().string().find("BuiltIn");
		if (!UHFbxImporterInterface->IsUHMeshCached(Idx->path())
			&& Found == std::string::npos)
		{
			// need reimport
			continue;
		}
#endif

		// try to import UHMesh, transfer to list if it's loaded successfully
		UniquePtr<UHMesh> LoadedMesh = MakeUnique<UHMesh>();
		if (LoadedMesh->Import(Idx->path()))
		{
			UHMeshes.push_back(std::move(LoadedMesh));
		}
	}

	// initialize the cache list as well
	UHMeshesCache.resize(UHMeshes.size());
	for (size_t Idx = 0; Idx < UHMeshesCache.size(); Idx++)
	{
		UHMeshesCache[Idx] = UHMeshes[Idx].get();
	}
}

void UHAssetManager::TranslateHLSL(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName, UHMaterialCompileData InData
	, std::vector<std::string> Defines, std::filesystem::path& OutputShaderPath)
{
#if WITH_EDITOR
	UHMaterial* InMat = InData.MaterialCache;
	UHMaterialCompileFlag CompileFlag = InMat->GetCompileFlag();

	if (CompileFlag == FullCompileTemporary
		|| CompileFlag == FullCompileResave
		|| !UHMaterialImporterInterface->IsMaterialCached(InMat, InShaderName, Defines)
		|| !UHShaderImporterInterface->IsShaderTemplateCached(InSource, EntryName, ProfileName))
	{
		// mark as include changed when necessary
		if (!UHShaderImporterInterface->IsShaderIncludeCached() && CompileFlag == UpToDate)
		{
			InMat->SetCompileFlag(IncludeChanged);
		}

		OutputShaderPath = UHShaderImporterInterface->TranslateHLSL(InShaderName, InSource, EntryName, ProfileName, InData, Defines);

		// don't write cache for temporrary compiliation
		if (CompileFlag != FullCompileTemporary)
		{
			UHMaterialImporterInterface->WriteMaterialCache(InMat, InShaderName, Defines);
		}
	}
#endif
}

void UHAssetManager::CompileShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
	, std::vector<std::string> Defines)
{
#if WITH_EDITOR
	UHShaderImporterInterface->CompileHLSL(InShaderName, InSource, EntryName, ProfileName, Defines);
#endif
}

void UHAssetManager::ImportTextures(UHGraphic* InGfx)
{
	// the same as mesh, import raw textures first, then UH texture
	if (!std::filesystem::exists(GTextureAssetFolder))
	{
		std::filesystem::create_directories(GTextureAssetFolder);
	}

	// load UH textures from asset folder
	for (std::filesystem::recursive_directory_iterator Idx(GTextureAssetFolder.c_str()), end; Idx != end; Idx++)
	{
		// skip directory
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}

		UHTexture2D LoadedTex;
		if (LoadedTex.Import(Idx->path()))
		{
			// import successfully, request texture 2d from GFX
			UHTexture2D* NewTex = InGfx->RequestTexture2D(LoadedTex);

#if WITH_EDITOR
			NewTex->SetRawSourcePath(LoadedTex.GetRawSourcePath());
#endif
			UHTexture2Ds.push_back(NewTex);
		}
	}
}

void UHAssetManager::ImportMaterials(UHGraphic* InGfx)
{
	// load all UHMaterials under asset folder
	if (!std::filesystem::exists(GMaterialAssetPath))
	{
		std::filesystem::create_directories(GMaterialAssetPath);
	}

	for (std::filesystem::recursive_directory_iterator Idx(GMaterialAssetPath.c_str()), end; Idx != end; Idx++)
	{
		// skip directory
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}

		UHMaterial* Mat = InGfx->RequestMaterial(Idx->path());
		if (Mat)
		{
			// map texture index after creation
			MapTextureIndex(Mat);
			UHMaterialsCache.push_back(Mat);
		}
	}
}

void UHAssetManager::MapTextureIndex(UHMaterial* InMat)
{
	if (InMat == nullptr)
	{
		return;
	}

	// set texture reference after material creation
	std::vector<int32_t> RegisteredIndexes;
	for (const std::string RegisteredTexture : InMat->GetRegisteredTextureNames())
	{
		for (int32_t Jdx = 0; Jdx < UHTexture2Ds.size(); Jdx++)
		{
			if (UHTexture2Ds[Jdx]->GetSourcePath() == RegisteredTexture 
#if WITH_EDITOR
				|| UHTexture2Ds[Jdx]->GetSourcePath() == FindTexturePathName(RegisteredTexture)
#endif
				)
			{
				// find referenced texture and set index
				// add to referenced texture list if doesn't exist
				int32_t TextureIdx = UHUtilities::FindIndex(ReferencedTexture2Ds, UHTexture2Ds[Jdx]);

				if (TextureIdx == -1)
				{
					TextureIdx = static_cast<int32_t>(ReferencedTexture2Ds.size());
					ReferencedTexture2Ds.push_back(UHTexture2Ds[Jdx]);
				}

				RegisteredIndexes.push_back(TextureIdx);
				UHTexture2Ds[Jdx]->AddReferenceObject(InMat);
				break;
			}
		}
	}

	InMat->SetRegisteredTextureIndexes(RegisteredIndexes);
}

std::vector<UHMesh*> UHAssetManager::GetUHMeshes() const
{
	return UHMeshesCache;
}

std::vector<UHMaterial*> UHAssetManager::GetMaterials() const
{
	return UHMaterialsCache;
}

std::vector<UHTexture2D*> UHAssetManager::GetTexture2Ds() const
{
	return UHTexture2Ds;
}

std::vector<UHTexture2D*> UHAssetManager::GetReferencedTexture2Ds() const
{
	return ReferencedTexture2Ds;
}

// get texture by name, only use for initialization
UHTexture2D* UHAssetManager::GetTexture2D(std::string InName) const
{
	for (UHTexture2D* Tex : UHTexture2Ds)
	{
		if (Tex->GetName() == InName || Tex->GetSourcePath() == InName)
		{
			return Tex;
		}
	}

	return nullptr;
}

// get texture by path, only use for initialization
UHTexture2D* UHAssetManager::GetTexture2DByPath(std::filesystem::path InPath) const
{
	for (UHTexture2D* Tex : UHTexture2Ds)
	{
		if (Tex->GetSourcePath() == InPath)
		{
			return Tex;
		}
	}

	return nullptr;
}

UHMaterial* UHAssetManager::GetMaterial(std::string InName) const
{
	// get material by name, don't use this except for initialization!
	for (UHMaterial* Mat : UHMaterialsCache)
	{
		if (Mat->GetName() == InName)
		{
			return Mat;
		}
	}

	return nullptr;
}

UHMesh* UHAssetManager::GetMesh(std::string InName) const
{
	// get mesh by name, don't use this except for initialization!
	for (UHMesh* Mesh : UHMeshesCache)
	{
		if (Mesh->GetName() == InName)
		{
			return Mesh;
		}
	}

	return nullptr;
}

#if WITH_EDITOR
void UHAssetManager::AddTexture2D(UHTexture2D* InTexture2D)
{
	if (!UHUtilities::FindByElement(UHTexture2Ds, InTexture2D))
	{
		UHTexture2Ds.push_back(InTexture2D);
	}
}

UHAssetManager* UHAssetManager::GetAssetMgrEditor()
{
	return AssetMgrEditorOnly;
}

UHTexture2D* UHAssetManager::GetTexture2DByPathEditor(std::string InPathName)
{
	if (AssetMgrEditorOnly == nullptr)
	{
		return nullptr;
	}

	for (UHTexture2D* Tex : AssetMgrEditorOnly->GetTexture2Ds())
	{
		if (Tex->GetSourcePath() == AssetMgrEditorOnly->FindTexturePathName(InPathName))
		{
			return Tex;
		}
	}

	return nullptr;
}

// find texture path name by name, used for old asset look-up
std::string UHAssetManager::FindTexturePathName(std::string InName)
{
	for (const UHTexture2D* Tex : AssetMgrEditorOnly->GetTexture2Ds())
	{
		if (Tex->GetName() == InName)
		{
			return Tex->GetSourcePath();
		}
	}

	return InName;
}
#endif
