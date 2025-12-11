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
	// load shader cache after initialization
	UHShaderImporterInterface = MakeUnique<UHShaderImporter>();
	UHShaderImporterInterface->LoadShaderCache();
	UHShader FallbackShader("FallbackPixelShader", GRawShaderPath + "FallbackPixelShader.hlsl", "FallbackPS", "ps_6_0", std::vector<std::string>());
	UHShaderImporterInterface->CompileHLSL(&FallbackShader);

	UHMaterialImporterInterface = MakeUnique<UHMaterialImporter>();
	UHMaterialImporterInterface->LoadMaterialCache();

	// generate built-in meshes
	UHMesh BuiltInCube = UHGeometryHelper::CreateCubeMesh();
	BuiltInCube.Export(GBuiltInMeshAssetPath, false);

	// generate built-in textures
	{
		const uint32_t FallbackWidth = 2;
		const uint32_t FallbackHeight = 2;
		const UHTextureFormat FallbackTexFormat = UHTextureFormat::UH_FORMAT_RGBA8_UNORM;

		UHTextureSettings TexSettings{};
		TexSettings.bIsLinear = true;
		TexSettings.bUseMipmap = false;

		VkExtent2D SystemTexSize;
		SystemTexSize.width = FallbackWidth;
		SystemTexSize.height = FallbackHeight;

		if (!std::filesystem::exists(GBuiltInTextureAssetPath))
		{
			std::filesystem::create_directories(GBuiltInTextureAssetPath);
		}

		// 2x2 white tex
		std::unique_ptr<UHTexture2D> SystemTex = MakeUnique<UHTexture2D>("UHWhiteTex", "UHWhiteTex", SystemTexSize, FallbackTexFormat, TexSettings);
		std::vector<uint8_t> TexData(2 * 2 * GTextureFormatData[UH_ENUM_VALUE(FallbackTexFormat)].ByteSize, 255);
		SystemTex->SetTextureData(TexData);
		SystemTex->Export(GBuiltInTextureAssetPath + SystemTex->GetName(), false);

		// 2x2 black tex, with alpha channel = 1
		SystemTex = MakeUnique<UHTexture2D>("UHBlackTex", "UHBlackTex", SystemTexSize, FallbackTexFormat, TexSettings);
		memset(TexData.data(), 0, TexData.size());
		for (size_t Idx = 0; Idx < TexData.size(); Idx += 4)
		{
			TexData[Idx + 3] = 255;
		}
		SystemTex->SetTextureData(TexData);
		SystemTex->Export(GBuiltInTextureAssetPath + SystemTex->GetName(), false);

		// 2x2 transparent tex, with all channels = 0
		memset(TexData.data(), 0, TexData.size());
		SystemTex = MakeUnique<UHTexture2D>("UHTransparentTex", "UHTransparentTex", SystemTexSize, FallbackTexFormat, TexSettings);
		SystemTex->SetTextureData(TexData);
		SystemTex->Export(GBuiltInTextureAssetPath + SystemTex->GetName(), false);

		// 2x2 black cube
		std::unique_ptr<UHTextureCube> SystemCube = MakeUnique<UHTextureCube>("UHBlackCube", SystemTexSize, FallbackTexFormat, TexSettings);
		SystemCube->SetSourcePath("UHBlackCube");
		for (int32_t Idx = 0; Idx < 6; Idx++)
		{
			SystemCube->SetCubeData(TexData, Idx);
		}
		SystemCube->Export(GBuiltInTextureAssetPath + SystemCube->GetName(), false);

		// 2x2 default sky
		SystemCube = MakeUnique<UHTextureCube>("UHDefaultSkyCube", SystemTexSize, FallbackTexFormat, TexSettings);
		SystemCube->SetSourcePath("UHDefaultSkyCube");
		for (size_t Idx = 0; Idx < TexData.size(); Idx += 4)
		{
			TexData[Idx] = 135;
			TexData[Idx + 1] = 206;
			TexData[Idx + 2] = 235;
			TexData[Idx + 3] = 255;
		}

		for (int32_t Idx = 0; Idx < 6; Idx++)
		{
			SystemCube->SetCubeData(TexData, Idx);
		}
		SystemCube->Export(GBuiltInTextureAssetPath + SystemCube->GetName(), false);
	}

	AssetMgrEditorOnly = this;
#else
	// load asset map during launch for release
	std::ifstream FileIn(GAssetPath + GAssetMapName, std::ios::in | std::ios::binary);
	if (FileIn.is_open())
	{
		size_t NumAssets;
		FileIn.read(reinterpret_cast<char*>(&NumAssets), sizeof(NumAssets));

		AllAssetsMap.resize(NumAssets);
		for (size_t Idx = 0; Idx < NumAssets; Idx++)
		{
			FileIn.read(reinterpret_cast<char*>(&AllAssetsMap[Idx].AssetUUid), sizeof(AllAssetsMap[Idx].AssetUUid));
			UHUtilities::ReadStringData(FileIn, AllAssetsMap[Idx].FilePath);
		}
	}
	FileIn.close();
#endif

	GfxCache = nullptr;
}

void UHAssetManager::SetGfxCache(UHGraphic* InGfx)
{
	GfxCache = InGfx;
}

void UHAssetManager::ImportBuiltInAssets()
{
	// import all built in assets

	// meshes
	std::filesystem::create_directories(GBuiltInMeshAssetPath);
	for (std::filesystem::recursive_directory_iterator Idx(GBuiltInMeshAssetPath), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}
		ImportAsset(Idx->path());
	}

	// textures
	std::filesystem::create_directories(GBuiltInTextureAssetPath);
	for (std::filesystem::recursive_directory_iterator Idx(GBuiltInTextureAssetPath), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}
		ImportAsset(Idx->path());
	}
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
		}
	}

	for (UHTexture2D* Tex : UHTexture2Ds)
	{
		GfxCache->RequestReleaseTexture2D(Tex);
	}

	for (UHTextureCube* Cube : UHCubemaps)
	{
		GfxCache->RequestReleaseTextureCube(Cube);
	}

	for (UHMaterial* Mat : UHMaterialsCache)
	{
		GfxCache->RequestReleaseMaterial(Mat);
	}

	for (UHAssetMap& Map : AllAssetsMap)
	{
		Map.Asset = nullptr;
	}

	// container cleanup
	ClearAssetCaches();
}

void UHAssetManager::TranslateHLSL(UHShader* InShader, UHMaterialCompileData InData, std::filesystem::path& OutputShaderPath)
{
#if WITH_EDITOR
	UHMaterial* InMat = InData.MaterialCache;
	UHMaterialCompileFlag CompileFlag = InMat->GetCompileFlag();

	if (CompileFlag == UHMaterialCompileFlag::FullCompileTemporary
		|| CompileFlag == UHMaterialCompileFlag::FullCompileResave
		|| !UHMaterialImporterInterface->IsMaterialCached(InMat, InShader)
		|| !UHShaderImporterInterface->IsShaderTemplateCached(InShader->GetSourcePath(), InShader->GetEntryName(), InShader->GetProfileName()))
	{
		// mark as include changed when necessary
		if (!UHShaderImporterInterface->IsShaderIncludeCached() && CompileFlag == UHMaterialCompileFlag::UpToDate)
		{
			InMat->SetCompileFlag(UHMaterialCompileFlag::IncludeChanged);
		}

		OutputShaderPath = UHShaderImporterInterface->TranslateHLSL(InShader, InData);

		// don't write cache for temporrary compiliation
		if (CompileFlag != UHMaterialCompileFlag::FullCompileTemporary)
		{
			UHMaterialImporterInterface->WriteMaterialCache(InMat, InShader);
		}
	}
#endif
}

void UHAssetManager::CompileShader(UHShader* InShader)
{
#if WITH_EDITOR
	UHShaderImporterInterface->CompileHLSL(InShader);
#endif
}

void UHAssetManager::ClearAssetCaches()
{
	UHMeshes.clear();
	UHMeshesCache.clear();
	UHMaterialsCache.clear();
	UHTexture2Ds.clear();
	ReferencedTexture2Ds.clear();
	UHCubemaps.clear();
}

UHObject* UHAssetManager::ImportMesh(std::filesystem::path InPath)
{
	UHObject* Result = nullptr;
	UniquePtr<UHMesh> LoadedMesh = MakeUnique<UHMesh>();

	if (LoadedMesh->Import(InPath))
	{
		UHMeshesCache.push_back(LoadedMesh.get());
		if (GIsEditor)
		{
			AllAssetsMap.push_back(UHAssetMap(LoadedMesh.get(), InPath.string()));
		}

		Result = LoadedMesh.get();
		UHMeshes.push_back(std::move(LoadedMesh));
	}

	return Result;
}

UHObject* UHAssetManager::ImportTexture(std::filesystem::path InPath)
{
	UHObject* Result = nullptr;
	UniquePtr<UHTexture2D> LoadedTex = MakeUnique<UHTexture2D>();

	if (LoadedTex->Import(InPath))
	{
		// import successfully, request texture 2d from GFX
		UHTexture2D* NewTex = GfxCache->RequestTexture2D(LoadedTex, true);
		UHTexture2Ds.push_back(NewTex);
		if (GIsEditor)
		{
			AllAssetsMap.push_back(UHAssetMap(NewTex, InPath.string()));
		}

		Result = NewTex;
	}

	return Result;
}

UHObject* UHAssetManager::ImportCubemap(std::filesystem::path InPath)
{
	UHObject* Result = nullptr;
	UniquePtr<UHTextureCube> LoadedCube = MakeUnique<UHTextureCube>();

	if (LoadedCube->Import(InPath))
	{
		// import successfully, request texture cube from GFX
		UHTextureInfo Info{};
		Info.Format = LoadedCube->GetFormat();
		Info.Extent = LoadedCube->GetExtent();

		UHTextureCube* NewCube = GfxCache->RequestTextureCube(LoadedCube);
		UHCubemaps.push_back(NewCube);
		if (GIsEditor)
		{
			AllAssetsMap.push_back(UHAssetMap(NewCube, InPath.string()));
		}

		Result = NewCube;
	}

	return Result;
}

UHObject* UHAssetManager::ImportMaterial(std::filesystem::path InPath)
{
	return AddImportedMaterial(InPath);
}

UHObject* UHAssetManager::ImportAsset(std::filesystem::path InPath)
{
	UHObject* Result = nullptr;
	const std::string Extension = InPath.extension().string();

	if (Extension == GMeshAssetExtension)
	{
		Result = ImportMesh(InPath);
	}
	else if (Extension == GTextureAssetExtension)
	{
		Result = ImportTexture(InPath);
	}
	else if (Extension == GCubemapAssetExtension)
	{
		Result = ImportCubemap(InPath);
	}
	else if (Extension == GMaterialAssetExtension)
	{
		Result = ImportMaterial(InPath);
	}

	return Result;
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
			if (std::filesystem::path(UHTexture2Ds[Jdx]->GetSourcePath()) == std::filesystem::path(RegisteredTexture)
#if WITH_EDITOR
				|| UHTexture2Ds[Jdx]->GetSourcePath() == FindTexturePathName(RegisteredTexture)
#endif
				)
			{
				// find referenced texture and set index
				// add to referenced texture list if doesn't exist
				int32_t TextureIdx = UHUtilities::FindIndex(ReferencedTexture2Ds, UHTexture2Ds[Jdx]);

				if (TextureIdx == UHINDEXNONE)
				{
					TextureIdx = static_cast<int32_t>(ReferencedTexture2Ds.size());
					ReferencedTexture2Ds.push_back(UHTexture2Ds[Jdx]);
				}

				// offset the texture index with system preserved texture slots
				TextureIdx += GSystemPreservedTextureSlots;
				RegisteredIndexes.push_back(TextureIdx);
				UHTexture2Ds[Jdx]->AddReferenceObject(InMat);
				break;
			}
		}
	}

	InMat->SetRegisteredTextureIndexes(RegisteredIndexes);
}

const std::vector<UHMesh*>& UHAssetManager::GetUHMeshes() const
{
	return UHMeshesCache;
}

const std::vector<UHMaterial*>& UHAssetManager::GetMaterials() const
{
	return UHMaterialsCache;
}

const std::vector<UHTexture2D*>& UHAssetManager::GetTexture2Ds() const
{
	return UHTexture2Ds;
}

const std::vector<UHTexture2D*>& UHAssetManager::GetReferencedTexture2Ds() const
{
	return ReferencedTexture2Ds;
}

const std::vector<UHTextureCube*>& UHAssetManager::GetCubemaps() const
{
	return UHCubemaps;
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

UHTextureCube* UHAssetManager::GetCubemapByName(std::string InName) const
{
	for (UHTextureCube* Tex : UHCubemaps)
	{
		if (Tex->GetName() == InName)
		{
			return Tex;
		}
	}

	return nullptr;
}

UHTextureCube* UHAssetManager::GetCubemapByPath(std::filesystem::path InPath) const
{
	for (UHTextureCube* Tex : UHCubemaps)
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
		if (Mat->GetName() == InName || Mat->GetSourcePath() == InName)
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
		if (Mesh->GetName() == InName || Mesh->GetSourcePath() == InName)
		{
			return Mesh;
		}
	}

	return nullptr;
}

UHObject* UHAssetManager::GetAsset(UUID InAssetUuid)
{
	for (UHAssetMap& AssetMap : AllAssetsMap)
	{
		if (AssetMap.AssetUUid == InAssetUuid)
		{
			if (AssetMap.Asset != nullptr)
			{
				// safely get the object if it's created already
				UHObject* Obj = SafeGetObjectFromTable<UHObject>(AssetMap.Asset->GetId());
				if (Obj && Obj->GetRuntimeGuid() == InAssetUuid)
				{
					return Obj;
				}
			}
			else
			{
				// load asset if not found and cache in the asset map
				UHObject* Obj = ImportAsset(AssetMap.FilePath);
				AssetMap.Asset = Obj;
				return Obj;
			}
		}
	}

	return nullptr;
}

UHObject* UHAssetManager::GetAsset(std::string InPath)
{
	for (UHAssetMap& AssetMap : AllAssetsMap)
	{
		if (std::filesystem::path(AssetMap.FilePath) == std::filesystem::path(InPath))
		{
			if (AssetMap.Asset != nullptr)
			{
				// safely get the object if it's created already
				UHObject* Obj = SafeGetObjectFromTable<UHObject>(AssetMap.Asset->GetId());
				if (Obj && Obj->GetRuntimeGuid() == AssetMap.AssetUUid)
				{
					return Obj;
				}
			}
			else
			{
				// load asset if not found and cache in the asset map
				UHObject* Obj = ImportAsset(AssetMap.FilePath);
				AssetMap.Asset = Obj;
				return Obj;
			}
		}
	}

	return nullptr;
}

UHObject* UHAssetManager::AddImportedMaterial(std::filesystem::path InPath)
{
	UHObject* Result = nullptr;
	UHMaterial* Mat = GfxCache->RequestMaterial(InPath);

	if (Mat)
	{
		// it's also important to update the texture references after material is imported
		for (const std::string& TextureName : Mat->GetRegisteredTextureNames())
		{
			GetAsset(GTextureAssetFolder + TextureName + GTextureAssetExtension);
		}
		MapTextureIndex(Mat);

		UHMaterialsCache.push_back(Mat);
		if (GIsEditor)
		{
			AllAssetsMap.push_back(UHAssetMap(Mat, InPath.string()));
		}

		Result = Mat;
	}

	return Result;
}

#if WITH_EDITOR
void UHAssetManager::ImportAssets()
{
	std::filesystem::create_directories(GMeshAssetFolder);
	std::filesystem::create_directories(GTextureAssetFolder);
	std::filesystem::create_directories(GMaterialAssetPath);

	ClearAssetCaches();
	AllAssetsMap.clear();

	for (std::filesystem::recursive_directory_iterator Idx(GAssetPath), end; Idx != end; Idx++)
	{
		// skip directory
		if (std::filesystem::is_directory(Idx->path()))
		{
			continue;
		}

		// load corresponding asset based on file type
		ImportAsset(Idx->path());
	}

	// output asset map after import all
	std::ofstream FileOut(GAssetPath + GAssetMapName, std::ios::out | std::ios::binary);

	size_t NumAssets = AllAssetsMap.size();
	FileOut.write(reinterpret_cast<const char*>(&NumAssets), sizeof(NumAssets));

	for (size_t Idx = 0; Idx < NumAssets; Idx++)
	{
		FileOut.write(reinterpret_cast<const char*>(&AllAssetsMap[Idx].AssetUUid), sizeof(AllAssetsMap[Idx].AssetUUid));
		UHUtilities::WriteStringData(FileOut, AllAssetsMap[Idx].FilePath);
	}

	FileOut.close();
}

void UHAssetManager::AddTexture2D(UHTexture2D* InTexture2D)
{
	if (!UHUtilities::FindByElement(UHTexture2Ds, InTexture2D))
	{
		UHTexture2Ds.push_back(InTexture2D);
	}
}

void UHAssetManager::AddImportedMesh(UniquePtr<UHMesh>& InMesh)
{
	UHMeshesCache.push_back(InMesh.get());
	UHMeshes.push_back(std::move(InMesh));
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
		if (Tex->GetSourcePath() == AssetMgrEditorOnly->FindTexturePathName(InPathName)
			|| std::filesystem::path(Tex->GetSourcePath()) == std::filesystem::path(InPathName))
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
		if (Tex->GetName() == InName || std::filesystem::path(Tex->GetSourcePath()) == std::filesystem::path(InName))
		{
			return Tex->GetSourcePath();
		}
	}

	return InName;
}

void UHAssetManager::AddCubemap(UHTextureCube* InCube)
{
	UHCubemaps.push_back(InCube);
}

UHShaderImporter* UHAssetManager::GetShaderImporter() const
{
	return UHShaderImporterInterface.get();
}
#endif
