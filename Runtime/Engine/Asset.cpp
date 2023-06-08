#include "Asset.h"
#include <fstream>
#include "../Classes/Utility.h"
#include "../Classes/Texture2D.h"
#include "Graphic.h"
#include "../Classes/AssetPath.h"

#if WITH_DEBUG
#include "../../Editor/Classes/GeometryUtility.h"
#include <chrono>
#endif

UHAssetManager::UHAssetManager()
{
#if WITH_DEBUG
	UHFbxImporterInterface = std::make_unique<UHFbxImporter>();
	UHTextureImporterInterface = std::make_unique<UHTextureImporter>();

	// load shader cache after initialization
	UHShaderImporterInterface = std::make_unique<UHShaderImporter>();
	UHShaderImporterInterface->LoadShaderCache();
	UHShaderImporterInterface->CompileHLSL("FallbackPixelShader", GRawShaderPath + "FallbackPixelShader.hlsl", "FallbackPS", "ps_6_0", std::vector<std::string>());

	UHMaterialImporterInterface = std::make_unique<UHMaterialImporter>();
	UHMaterialImporterInterface->LoadMaterialCache();

	// generate built-in meshes
	UHMesh BuiltInCube = UHGeometryHelper::CreateCubeMesh();
	BuiltInCube.Export(GBuiltInMeshAssetPath, false);

#endif
}

void UHAssetManager::Release()
{
	// release meshes
	for (auto& Mesh : UHMeshes)
	{
		Mesh->ReleaseCPUMeshData();
		Mesh->Release();
		Mesh.reset();
	}

#if WITH_DEBUG
	// write shader include cache when exitng
	UHShaderImporterInterface->WriteShaderIncludeCache();
	UHShaderImporterInterface.reset();
	UHFbxImporterInterface.reset();
	UHTextureImporterInterface.reset();
	UHMaterialImporterInterface.reset();
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
#if WITH_DEBUG
	std::vector<std::unique_ptr<UHMaterial>> ImportedMat;
	UHFbxImporterInterface->ImportRawFbx(ImportedMat);

	// FBX will also import material, so export as UH material right after import
	for (std::unique_ptr<UHMaterial>& Mat : ImportedMat)
	{
		Mat->GenerateDefaultMaterialNodes();
		Mat->Export();
	}

	for (std::unique_ptr<UHMaterial>& Mat : ImportedMat)
	{
		Mat.reset();
	}
	ImportedMat.clear();
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

#if WITH_DEBUG
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
		std::unique_ptr<UHMesh> LoadedMesh = std::make_unique<UHMesh>();
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
#if WITH_DEBUG
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
#if WITH_DEBUG
	UHShaderImporterInterface->CompileHLSL(InShaderName, InSource, EntryName, ProfileName, Defines);
#endif
}

void UHAssetManager::ImportTextures(UHGraphic* InGfx)
{
	// import raw texture first
	// cached texture won't be imported again
#if WITH_DEBUG
	UHTextureImporterInterface->LoadCaches();
	UHTextureImporterInterface->ImportRawTextures();
#endif

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
		
	#if WITH_DEBUG
		if (!UHTextureImporterInterface->IsUHTextureCached(Idx->path()))
		{
			// need reimport
			continue;
		}
	#endif

		UHTexture2D LoadedTex;
		if (LoadedTex.Import(Idx->path()))
		{
			// import successfully, request texture 2d from GFX
			VkExtent2D Extent = LoadedTex.GetExtent();

			UHTexture2D* NewTex = InGfx->RequestTexture2D(Idx->path().stem().string(), LoadedTex.GetSourcePath()
				, Extent.width, Extent.height, LoadedTex.GetTextureData(), LoadedTex.IsLinear());
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
			// set texture reference after material creation
			for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
			{
				UHMaterialTextureType TexType = static_cast<UHMaterialTextureType>(Idx);
				std::string TexName = Mat->GetTexFileName(TexType);

				for (int32_t Jdx = 0; Jdx < UHTexture2Ds.size(); Jdx++)
				{
					if (UHTexture2Ds[Jdx]->GetName() == TexName)
					{
						// find referenced texture and set index
						// add to referenced texture list if doesn't exist
						int32_t TextureIdx = UHUtilities::FindIndex(ReferencedTexture2Ds, UHTexture2Ds[Jdx]);

						if (TextureIdx == -1)
						{
							TextureIdx = static_cast<int32_t>(ReferencedTexture2Ds.size());
							ReferencedTexture2Ds.push_back(UHTexture2Ds[Jdx]);
						}

						Mat->SetTextureIndex(TexType, TextureIdx);

						break;
					}
				}
			}

			UHMaterialsCache.push_back(Mat);
		}
	}
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
		if (Tex->GetName() == InName)
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