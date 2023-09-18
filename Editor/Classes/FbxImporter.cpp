#include "FbxImporter.h"

#if WITH_EDITOR
#include <fstream>
#include "../Runtime/Classes/Utility.h"
#include "../Runtime/Classes/AssetPath.h"

UHFbxImporter::UHFbxImporter()
{
	// create fbx manager
	FbxSDKManager = FbxManager::Create();

	// load mesh caches
	LoadCaches();
}

UHFbxImporter::~UHFbxImporter()
{
	// destroy fbx manager
	FbxSDKManager->Destroy();
	FbxSDKManager = nullptr;
}

void UHFbxImporter::ImportRawFbx(std::vector<UniquePtr<UHMaterial>>& InMatVector)
{
	// IO settings initialization
	FbxIOSettings* FbxSDKIOSettings = FbxIOSettings::Create(FbxSDKManager, IOSROOT);
	FbxSDKManager->SetIOSettings(FbxSDKIOSettings);

	// check raw asset folder
	if (!std::filesystem::exists(GRawMeshAssetPath))
	{
		std::filesystem::create_directories(GRawMeshAssetPath);
		UHE_LOG(L"Empty mesh asset folder, please put some FBX files.\n");
	}

	ImportedMaterialNames.clear();

	// importer initialization
	for (std::filesystem::recursive_directory_iterator Idx(GRawMeshAssetPath.c_str()), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()) || IsCached(Idx->path()) || !UHAssetPath::IsTheSameExtension(Idx->path(), GRawMeshAssetExtension))
		{
			// skip directory or cached raw assets
			continue;
		}

		// create fbx importer
		FbxImporter* FbxSDKImporter = FbxImporter::Create(FbxSDKManager, "UHFbxImporter");
		if (!FbxSDKImporter->Initialize(Idx->path().string().c_str(), -1, FbxSDKManager->GetIOSettings()))
		{
			// it could be here if non-fbx file was found
			UHE_LOG(L"Failed to load " + Idx->path().wstring() + L"\n");
			continue;
		}

		UHE_LOG(L"Success to initialize " + Idx->path().wstring() + L"\n");

		// prepare mesh cache, cache the source path, and modified time
		// the system will decide if to import based on the modified time
		UHRawMeshAssetCache Cache{};
		Cache.SourcePath = Idx->path();
		Cache.SourcLastModifiedTime = std::filesystem::last_write_time(Idx->path()).time_since_epoch().count();

		// Import the contents of the file into the scene.
		FbxScene* Scene = FbxScene::Create(FbxSDKManager, "UHFbxScene");
		if (FbxSDKImporter->Import(Scene))
		{
			// force cm
			FbxSystemUnit::cm.ConvertScene(Scene);

			// force Y up
			FbxAxisSystem::MayaYUp.ConvertScene(Scene);

			// force breaking into single mesh
			FbxGeometryConverter GeoConverter(FbxSDKManager);
			GeoConverter.SplitMeshesPerMaterial(Scene, true);

			// create UHMeshes after importing
			CreateUHMeshes(Scene->GetRootNode(), Idx->path(), InMatVector, Cache);
		}

		FbxSDKImporter->Destroy();
		Scene->Destroy();

		AddRawMeshAssetCache(Cache);
	}

	// finish importing, destroy them
	FbxSDKIOSettings->Destroy();
	FbxSDKIOSettings = nullptr;
	ImportedMaterialNames.clear();

	// output added caches
	OutputCaches();
}

bool UHFbxImporter::IsUHMeshCached(std::filesystem::path InUHMeshAssetPath)
{
	for (const UHRawMeshAssetCache& MeshCahe : MeshAssetCaches)
	{
		if (std::filesystem::exists(MeshCahe.SourcePath))
		{
			for (size_t Jdx = 0; Jdx < MeshCahe.UHMeshesPath.size(); Jdx++)
			{
				if (MeshCahe.UHMeshesPath[Jdx] == InUHMeshAssetPath)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// reav vertices and indices based on element UV index array
bool ReconstructVerticesAndIndices(FbxMesh* InMesh, std::vector<XMFLOAT3>& OutVertices, std::vector<uint32_t>& OutIndices)
{
	FbxGeometryElementUV* UVElement = InMesh->GetElementUV(0, FbxLayerElement::eTextureDiffuse);
	if (!UVElement)
	{
		// try creating UVs if there isn't 
		UVElement = InMesh->CreateElementUV(0, FbxLayerElement::eTextureDiffuse);
	}

	if (UVElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex && UVElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
	{
		UHE_LOG(L"Unsupported UV mapping mode.\n");
		return false;
	}

	const bool bUseIndex = UVElement->GetReferenceMode() != FbxGeometryElement::eDirect;
	const int32_t IndexCount = (bUseIndex) ? UVElement->GetIndexArray().GetCount() : 0;
	const int32_t PolyCount = InMesh->GetPolygonCount();
	const int32_t VertexCount = UVElement->GetDirectArray().GetCount();
	const FbxVector4* MeshPos = InMesh->GetControlPoints();
	const int32_t OldVertexCount = static_cast<int32_t>(OutVertices.size());

	// no need to read VB based on UV information if it's already correct
	if (OldVertexCount == VertexCount)
	{
		return false;
	}

	// inconsistent VB/IB should be with polygon vertex mode only
	if (UVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		OutVertices.clear();
		OutVertices.resize(VertexCount);
		OutIndices.clear();
		OutIndices.resize(IndexCount);

		// fill vertex by polygon
		int32_t PolyIndexCounter = 0;
		for (int32_t PolyIndex = 0; PolyIndex < PolyCount; PolyIndex++)
		{
			const int32_t PolySize = InMesh->GetPolygonSize(PolyIndex);
			for (int32_t VertIndex = 0; VertIndex < PolySize; VertIndex++)
			{
				int32_t PosIndex = bUseIndex ? UVElement->GetIndexArray().GetAt(PolyIndexCounter) : PolyIndexCounter;
				int32_t ControlPointIdx = InMesh->GetPolygonVertex(PolyIndex, VertIndex);

				XMFLOAT3 NewPos;
				NewPos.x = static_cast<float>(MeshPos[ControlPointIdx][0]);
				NewPos.y = static_cast<float>(MeshPos[ControlPointIdx][1]);
				NewPos.z = static_cast<float>(MeshPos[ControlPointIdx][2]);
				OutVertices[PosIndex] = NewPos;

				// fill index too if it has no index buffer
				if (!bUseIndex)
				{
					OutIndices[PolyIndexCounter] = PolyIndexCounter;
				}
				PolyIndexCounter++;
			}
		}

		// fill index
		for (int32_t Idx = 0; Idx < IndexCount; Idx++)
		{
			OutIndices[Idx] = UVElement->GetIndexArray().GetAt(Idx);
		}
	}

	return true;
}

// read uv information, modification is made for outputing the same number UVs as vertexes
// reference: https://help.autodesk.com/view/FBX/2016/ENU/?guid=__cpp_ref__import_scene_2_display_mesh_8cxx_example_html
std::vector<XMFLOAT2> ReadUVs(FbxMesh* InMesh, int32_t VertexCount, bool bMapToReconstruct, const std::vector<uint32_t>& Indices)
{
	// output the same counts as vertex buffer
	std::vector<XMFLOAT2> Result;
	Result.resize(VertexCount);

	FbxGeometryElementUV* UVElement = InMesh->GetElementUV(0, FbxLayerElement::eTextureDiffuse);
	if (!UVElement)
	{
		// try creating UVs if there isn't 
		UVElement = InMesh->CreateElementUV(0, FbxLayerElement::eTextureDiffuse);
	}

	if (UVElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex && UVElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
	{
		UHE_LOG(L"Unsupported UV mapping mode.\n");
		return Result;
	}

	// index array, where holds the index referenced to the uv data
	const bool bUseIndex = UVElement->GetReferenceMode() != FbxGeometryElement::eDirect;
	const int32_t IndexCount = (bUseIndex) ? UVElement->GetIndexArray().GetCount() : 0;
	const int32_t PolyCount = InMesh->GetPolygonCount();

	if (UVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
	{
		// mapping by control point simply loops vertex count
		for (int32_t ControlIdx = 0; ControlIdx < VertexCount; ControlIdx++)
		{
			FbxVector2 UVValue;

			// the index depends on the reference mode
			int32_t UVIndex = bUseIndex ? UVElement->GetIndexArray().GetAt(ControlIdx) : ControlIdx;
			UVValue = UVElement->GetDirectArray().GetAt(UVIndex);

			// output to corresponding location, inverse UV y for Vulkan spec
			Result[UVIndex].x = static_cast<float>(UVValue[0]);
			Result[UVIndex].y = 1.0f - static_cast<float>(UVValue[1]);
		}
	}
	else if (UVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		int32_t PolyIndexCounter = 0;
		for (int32_t PolyIndex = 0; PolyIndex < PolyCount; PolyIndex++)
		{
			const int32_t PolySize = InMesh->GetPolygonSize(PolyIndex);
			for (int32_t VertIndex = 0; VertIndex < PolySize; VertIndex++)
			{
				FbxVector2 UVValue;

				//the UV index depends on the reference mode
				int32_t UVIndex = bUseIndex ? UVElement->GetIndexArray().GetAt(PolyIndexCounter) : PolyIndexCounter;
				UVValue = UVElement->GetDirectArray().GetAt(UVIndex);
				PolyIndexCounter++;

				// output to corresponding location, inverse UV y for Vulkan spec
				if (bMapToReconstruct)
				{
					int32_t OutIdx = bUseIndex ? UVIndex : Indices[UVIndex];
					Result[OutIdx].x = static_cast<float>(UVValue[0]);
					Result[OutIdx].y = 1.0f - static_cast<float>(UVValue[1]);
				}
				else
				{
					int32_t ControlIdx = InMesh->GetPolygonVertex(PolyIndex, VertIndex);
					Result[ControlIdx].x = static_cast<float>(UVValue[0]);
					Result[ControlIdx].y = 1.0f - static_cast<float>(UVValue[1]);
				}
			}
		}
	}

	return Result;
}

// the same method as read uvs but it's for normals
std::vector<XMFLOAT3> ReadNormals(FbxMesh* InMesh, int32_t VertexCount, bool bMapToReconstruct, const std::vector<uint32_t>& Indices)
{
	// output the same counts as vertex buffer
	std::vector<XMFLOAT3> Result;
	Result.resize(VertexCount);

	const FbxGeometryElementNormal* NormalElement = InMesh->GetElementNormal(0);
	if (!NormalElement)
	{
		// try creating normals if there isn't
		NormalElement = InMesh->CreateElementNormal();
	}

	if (NormalElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex && NormalElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
	{
		UHE_LOG(L"Unsupported normal mapping mode.\n");
		return Result;
	}

	// index array, where holds the index referenced to the uv data
	const bool bUseIndex = NormalElement->GetReferenceMode() != FbxGeometryElement::eDirect;
	const int32_t IndexCount = (bUseIndex) ? NormalElement->GetIndexArray().GetCount() : 0;
	const int32_t PolyCount = InMesh->GetPolygonCount();

	if (NormalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
	{
		// mapping by control point simply loops vertex count
		for (int32_t ControlIdx = 0; ControlIdx < VertexCount; ControlIdx++)
		{
			FbxVector4 NormalValue;

			// the index depends on the reference mode
			int32_t NormalIndex = bUseIndex ? NormalElement->GetIndexArray().GetAt(ControlIdx) : ControlIdx;
			NormalValue = NormalElement->GetDirectArray().GetAt(NormalIndex);

			// output to corresponding location
			Result[NormalIndex].x = static_cast<float>(NormalValue[0]);
			Result[NormalIndex].y = static_cast<float>(NormalValue[1]);
			Result[NormalIndex].z = static_cast<float>(NormalValue[2]);
		}
	}
	else if (NormalElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		int32_t PolyIndexCounter = 0;
		for (int32_t PolyIndex = 0; PolyIndex < PolyCount; PolyIndex++)
		{
			const int32_t PolySize = InMesh->GetPolygonSize(PolyIndex);
			for (int32_t VertIndex = 0; VertIndex < PolySize; VertIndex++)
			{
				FbxVector4 NormalValue;

				// the index depends on the reference mode
				int32_t NormalIndex = bUseIndex ? NormalElement->GetIndexArray().GetAt(PolyIndexCounter) : PolyIndexCounter;
				NormalValue = NormalElement->GetDirectArray().GetAt(NormalIndex);
				PolyIndexCounter++;

				// output to corresponding location
				if (bMapToReconstruct)
				{
					int32_t OutIdx = bUseIndex ? NormalIndex : Indices[NormalIndex];
					Result[OutIdx].x = static_cast<float>(NormalValue[0]);
					Result[OutIdx].y = static_cast<float>(NormalValue[1]);
					Result[OutIdx].z = static_cast<float>(NormalValue[2]);
				}
				else
				{
					int32_t ControlIdx = InMesh->GetPolygonVertex(PolyIndex, VertIndex);
					Result[ControlIdx].x = static_cast<float>(NormalValue[0]);
					Result[ControlIdx].y = static_cast<float>(NormalValue[1]);
					Result[ControlIdx].z = static_cast<float>(NormalValue[2]);
				}
			}
		}
	}

	return Result;
}

std::vector<XMFLOAT4> ReadTangents(FbxMesh* InMesh, int32_t VertexCount, bool bMapToReconstruct, const std::vector<uint32_t>& Indices)
{
	// output the same counts as vertex buffer
	std::vector<XMFLOAT4> Result;
	Result.resize(VertexCount);

	const FbxGeometryElementTangent* TangentElement = InMesh->GetElementTangent(0);
	if (!TangentElement)
	{
		// try creating Tangents if there isn't
		TangentElement = InMesh->CreateElementTangent();
	}

	if (TangentElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex && TangentElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
	{
		UHE_LOG(L"Unsupported Tangent mapping mode.\n");
		return Result;
	}

	// index array, where holds the index referenced to the uv data
	const bool bUseIndex = TangentElement->GetReferenceMode() != FbxGeometryElement::eDirect;
	const int32_t IndexCount = (bUseIndex) ? TangentElement->GetIndexArray().GetCount() : 0;
	const int32_t PolyCount = InMesh->GetPolygonCount();

	if (TangentElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
	{
		// mapping by control point simply loops vertex count
		for (int32_t ControlIdx = 0; ControlIdx < VertexCount; ControlIdx++)
		{
			FbxVector4 TangentValue;

			// the index depends on the reference mode
			int32_t TangentIndex = bUseIndex ? TangentElement->GetIndexArray().GetAt(ControlIdx) : ControlIdx;
			TangentValue = TangentElement->GetDirectArray().GetAt(TangentIndex);

			// output to corresponding location
			Result[TangentIndex].x = static_cast<float>(TangentValue[0]);
			Result[TangentIndex].y = static_cast<float>(TangentValue[1]);
			Result[TangentIndex].z = static_cast<float>(TangentValue[2]);
			Result[TangentIndex].w = static_cast<float>(TangentValue[3]);
		}
	}
	else if (TangentElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
	{
		int32_t PolyIndexCounter = 0;
		for (int32_t PolyIndex = 0; PolyIndex < PolyCount; PolyIndex++)
		{
			const int32_t PolySize = InMesh->GetPolygonSize(PolyIndex);
			for (int32_t VertIndex = 0; VertIndex < PolySize; VertIndex++)
			{
				FbxVector4 TangentValue;

				// the index depends on the reference mode
				int32_t TangentIndex = bUseIndex ? TangentElement->GetIndexArray().GetAt(PolyIndexCounter) : PolyIndexCounter;
				TangentValue = TangentElement->GetDirectArray().GetAt(TangentIndex);
				PolyIndexCounter++;

				// output to corresponding location
				if (bMapToReconstruct)
				{
					int32_t OutIdx = bUseIndex ? TangentIndex : Indices[TangentIndex];
					Result[OutIdx].x = static_cast<float>(TangentValue[0]);
					Result[OutIdx].y = static_cast<float>(TangentValue[1]);
					Result[OutIdx].z = static_cast<float>(TangentValue[2]);
					Result[OutIdx].w = static_cast<float>(TangentValue[3]);
				}
				else
				{
					int32_t ControlIdx = InMesh->GetPolygonVertex(PolyIndex, VertIndex);
					Result[ControlIdx].x = static_cast<float>(TangentValue[0]);
					Result[ControlIdx].y = static_cast<float>(TangentValue[1]);
					Result[ControlIdx].z = static_cast<float>(TangentValue[2]);
					Result[ControlIdx].w = static_cast<float>(TangentValue[3]);
				}
			}
		}
	}

	return Result;
}

// mirror from FBX example: https://help.autodesk.com/view/FBX/2016/ENU/?guid=__cpp_ref__import_scene_2_display_material_8cxx_example_html
UniquePtr<UHMaterial> ImportMaterial(FbxNode* InNode)
{
	UniquePtr<UHMaterial> UHMat = MakeUnique<UHMaterial>();
	int32_t MatCount = InNode->GetMaterialCount();

	// only import single material per mesh at the moment
	if (MatCount > 0)
	{
		FbxPropertyT<FbxDouble3> FbxDouble3;
		FbxPropertyT<FbxDouble> FbxDouble1;
		FbxColor Color;

		FbxSurfaceMaterial* Mat = InNode->GetMaterial(0);
		std::wstring MatName = UHUtilities::ToStringW(Mat->GetName());
		UHMat->SetName(Mat->GetName());

		// extract matarial props that are suitable for UH
		UHMaterialProperty MatProps;

		if (Mat->GetClassId().Is(FbxSurfacePhong::ClassId))
		{
			// diffuse
			FbxDouble3 = ((FbxSurfacePhong*)Mat)->Diffuse;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->DiffuseFactor;
			MatProps.Diffuse.x = static_cast<float>(FbxDouble3.Get()[0] * FbxDouble1);
			MatProps.Diffuse.y = static_cast<float>(FbxDouble3.Get()[1] * FbxDouble1);
			MatProps.Diffuse.z = static_cast<float>(FbxDouble3.Get()[2] * FbxDouble1);

			// opacity
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->TransparencyFactor;
			MatProps.Opacity = static_cast<float>(FbxDouble1.Get());

			// emissive
			FbxDouble3 = ((FbxSurfacePhong*)Mat)->Emissive;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->EmissiveFactor;
			MatProps.Emissive.x = static_cast<float>(FbxDouble3.Get()[0] * FbxDouble1);
			MatProps.Emissive.y = static_cast<float>(FbxDouble3.Get()[1] * FbxDouble1);
			MatProps.Emissive.z = static_cast<float>(FbxDouble3.Get()[2] * FbxDouble1);

			// roughness
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->Shininess;
			MatProps.Roughness = 1.0f - static_cast<float>(FbxDouble1.Get() / 255.0f);

			// specular color
			FbxDouble3 = ((FbxSurfacePhong*)Mat)->Specular;
			MatProps.Specular.x = static_cast<float>(FbxDouble3.Get()[0]);
			MatProps.Specular.y = static_cast<float>(FbxDouble3.Get()[1]);
			MatProps.Specular.z = static_cast<float>(FbxDouble3.Get()[2]);

			// bump scale
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->BumpFactor;
			MatProps.BumpScale = static_cast<float>(FbxDouble1.Get());

			// Display the Reflectivity
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->ReflectionFactor;
			MatProps.ReflectionFactor = static_cast<float>(FbxDouble1.Get());
		}
		else if (Mat->GetClassId().Is(FbxSurfaceLambert::ClassId))
		{
			// diffuse
			FbxDouble3 = ((FbxSurfacePhong*)Mat)->Diffuse;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->DiffuseFactor;
			MatProps.Diffuse.x = static_cast<float>(FbxDouble3.Get()[0] * FbxDouble1);
			MatProps.Diffuse.y = static_cast<float>(FbxDouble3.Get()[1] * FbxDouble1);
			MatProps.Diffuse.z = static_cast<float>(FbxDouble3.Get()[2] * FbxDouble1);

			// opacity
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->TransparencyFactor;
			MatProps.Opacity = static_cast<float>(FbxDouble1.Get());

			// emissive
			FbxDouble3 = ((FbxSurfacePhong*)Mat)->Emissive;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->EmissiveFactor;
			MatProps.Emissive.x = static_cast<float>(FbxDouble3.Get()[0] * FbxDouble1);
			MatProps.Emissive.y = static_cast<float>(FbxDouble3.Get()[1] * FbxDouble1);
			MatProps.Emissive.z = static_cast<float>(FbxDouble3.Get()[2] * FbxDouble1);

			// bump scale
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->BumpFactor;
			MatProps.BumpScale = static_cast<float>(FbxDouble1.Get());
		}
		else
		{
			UHE_LOG(L"Unsupported material: " + MatName + L". This won't be imported. (Use Phong or Lambert)\n");
		}

		if (MatCount > 1)
		{
			UHE_LOG(L"Multiple materials detect: " + MatName + L". Only first material will be used.\n");
		}

		UHMat->SetMaterialProps(MatProps);

		// try to iterate textures, reference: https://help.autodesk.com/view/FBX/2016/ENU/?guid=__cpp_ref__import_scene_2_display_texture_8cxx_example_html
		int32_t TextureIndex;
		FBXSDK_FOR_EACH_TEXTURE(TextureIndex)
		{
			FbxProperty Property = Mat->FindProperty(FbxLayerElement::sTextureChannelNames[TextureIndex]);
			if (Property.IsValid())
			{
				int32_t TextureCount = Property.GetSrcObjectCount<FbxTexture>();
				for (int32_t Idx = 0; Idx < TextureCount; Idx++)
				{
					// skip layred texture for now
					FbxLayeredTexture* LayeredTexture = Property.GetSrcObject<FbxLayeredTexture>(Idx);
					if (LayeredTexture)
					{
						UHE_LOG(L"Layered texture is detected. It's currently not supported by UH.\n");
						continue;
					}

					// get regular texture info
					// @TODO: Set texture file name with path instead the name only
					FbxTexture* Texture = Property.GetSrcObject<FbxTexture>(Idx);
					if (Texture)
					{
						// get texture type and filename
						std::string TexType = Property.GetNameAsCStr();
						FbxFileTexture* FileTexture = FbxCast<FbxFileTexture>(Texture);
						std::filesystem::path TexFileName = FileTexture->GetFileName();
						
						if (TexType == "DiffuseColor")
						{
							UHMat->SetTexFileName(UHMaterialInputs::Diffuse, TexFileName.stem().string());
						}
						else if (TexType == "DiffuseFactor")
						{
							// treat diffuse factor map as AO map
							UHMat->SetTexFileName(UHMaterialInputs::Occlusion, TexFileName.stem().string());
						}
						else if (TexType == "SpecularColor")
						{
							UHMat->SetTexFileName(UHMaterialInputs::Specular, TexFileName.stem().string());
						}
						else if (TexType == "NormalMap" || TexType == "Bump")
						{
							// old Bump map (gray) will be used as normal in UH
							// always ask artists use Normal map
							UHMat->SetTexFileName(UHMaterialInputs::Normal, TexFileName.stem().string());
						}
						else if (TexType == "TransparentColor")
						{
							UHMat->SetTexFileName(UHMaterialInputs::Opacity, TexFileName.stem().string());

							// default to masked object if it contains opacity texture
							UHMat->SetBlendMode(UHBlendMode::Masked);
						}
						else
						{
							UHE_LOG(L"Unsupported texture type detected: " + UHUtilities::ToStringW(TexType) + L". This one will be skipped.\n");
						}
					}
				}
			}
		}
	}

	return UHMat;
}

void UHFbxImporter::CreateUHMeshes(FbxNode* InNode, std::filesystem::path InPath, std::vector<UniquePtr<UHMaterial>>& InMatVector, UHRawMeshAssetCache& Cache)
{
	// model loading here is straight forward, I only load what I need at the moment
	// if normal/tangent data is missing, I'd rely on Fbx's generation only
	// vertices and indices are loaded separaterly, so GetPolygonVertices() is the only polygon usage here
	// this fits graphic API's vertex/index buffer concept well

	// check if it's mesh node
	bool bIsMeshNode = false;
	for (int32_t Idx = 0; Idx < InNode->GetNodeAttributeCount(); Idx++)
	{
		if (InNode->GetNodeAttributeByIndex(Idx)->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			bIsMeshNode = true;
			break;
		}
	}

	if (bIsMeshNode)
	{
		std::wstring NodeNameW = UHUtilities::ToStringW(InNode->GetName());
		FbxMesh* InMesh = InNode->GetMesh();

		// convert to triangle if it's not
		if (!InMesh->IsTriangleMesh())
		{
			// meshes could be non-triangle, triangulating them!
			// **warning: This is a time-consuming operation, try to ask artists export as triangle before importing here, I really want to get rid of this call lol
			FbxGeometryConverter GeoConverter(FbxSDKManager);
			FbxNodeAttribute* NewNode = GeoConverter.Triangulate(InNode->GetNodeAttribute(), true);
			InMesh = NewNode->GetNode()->GetMesh();
		}

		// get proper transformation from FBX, reference: https://stackoverflow.com/questions/34452946/how-can-i-get-the-correct-position-of-fbx-mesh
		FbxAMatrix MatrixGeo;
		MatrixGeo.SetIdentity();
		const FbxVector4 LT = InNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		const FbxVector4 LR = InNode->GetGeometricRotation(FbxNode::eSourcePivot);
		const FbxVector4 LS = InNode->GetGeometricScaling(FbxNode::eSourcePivot);
		MatrixGeo.SetT(LT);
		MatrixGeo.SetR(LR);
		MatrixGeo.SetS(LS);
		FbxAMatrix LocalMatrix = InNode->EvaluateLocalTransform();

		FbxNode* ParentNode = InNode->GetParent();
		FbxAMatrix ParentMatrix;
		ParentMatrix.SetIdentity();
		if (ParentNode)
		{
			ParentMatrix = ParentNode->EvaluateLocalTransform();
		}

		while ((ParentNode = ParentNode->GetParent()) != NULL)
		{
			ParentMatrix = ParentNode->EvaluateLocalTransform() * ParentMatrix;
		}

		FbxAMatrix FinalMatrix = ParentMatrix * LocalMatrix * MatrixGeo;
		FbxVector4 FinalPos = FinalMatrix.GetT();
		FbxVector4 FinalRot = FinalMatrix.GetR();
		FbxVector4 FinalScale = FinalMatrix.GetS();

		// only create UH mesh if it's mesh node and valid transform
		UHMesh NewMesh(InNode->GetName());

		// prepare vertex data
		int32_t VertexCount = InMesh->GetControlPointsCount();

		// simply get all control points as vertices
		FbxVector4* MeshPos = InMesh->GetControlPoints();
		std::vector<XMFLOAT3> MeshVertices;
		for (int32_t Idx = 0; Idx < VertexCount; Idx++)
		{
			XMFLOAT3 Pos;
			Pos.x = static_cast<float>(MeshPos[Idx][0]);
			Pos.y = static_cast<float>(MeshPos[Idx][1]);
			Pos.z = static_cast<float>(MeshPos[Idx][2]);
			MeshVertices.push_back(Pos);
		}

		// basic info fetched, start to add indices
		std::vector<uint32_t> MeshIndices;
		for (int32_t Idx = 0; Idx < InMesh->GetPolygonCount(); Idx++)
		{
			// get index to control point and push them, after Triangulate call here is guaranteed to be triangle
			MeshIndices.push_back(InMesh->GetPolygonVertex(Idx, 0));
			MeshIndices.push_back(InMesh->GetPolygonVertex(Idx, 1));
			MeshIndices.push_back(InMesh->GetPolygonVertex(Idx, 2));
		}

		// when fetching other data like normal/uv, always mapping to control point, I'll store index to another array
		// for now, if data is missing, I'll simply use FBX function for creating them
		
		// try read VB/IB again for some special cases, e.g. Having 81 control points but having 102 UVs? Need to reconstruct the VB/IB based on UV.
		bool bIsReconstruct = ReconstructVerticesAndIndices(InMesh, MeshVertices, MeshIndices);
		NewMesh.SetIndicesData(MeshIndices);
		VertexCount = static_cast<int32_t>(MeshVertices.size());

		// get UV
		std::vector<XMFLOAT2> MeshUV0 = ReadUVs(InMesh, VertexCount, bIsReconstruct, MeshIndices);

		// get normal, and ensure it's not zero vector
		std::vector<XMFLOAT3> MeshNormal = ReadNormals(InMesh, VertexCount, bIsReconstruct, MeshIndices);
		for (XMFLOAT3& N : MeshNormal)
		{
			if (MathHelpers::IsVectorNearlyZero(N))
			{
				N = XMFLOAT3(0.0001f, 0.0001f, 0.0001f);
			}
		}

		// get tangent
		std::vector<XMFLOAT4> MeshTangent = ReadTangents(InMesh, VertexCount, bIsReconstruct, MeshIndices);
		for (XMFLOAT4& T : MeshTangent)
		{
			if (MathHelpers::IsVectorNearlyZero(T))
			{
				T = XMFLOAT4(0.0001f, 0.0001f, 0.0001f, 0.0001f);
			}
		}

		// setup imported transform at the end
		XMFLOAT3 Pos = XMFLOAT3(static_cast<float>(FinalPos[0]), static_cast<float>(FinalPos[1]), static_cast<float>(FinalPos[2]));
		XMFLOAT3 Rot = XMFLOAT3(static_cast<float>(FinalRot[0]), static_cast<float>(FinalRot[1]), static_cast<float>(FinalRot[2]));
		XMFLOAT3 Scale = XMFLOAT3(static_cast<float>(FinalScale[0]), static_cast<float>(FinalScale[1]), static_cast<float>(FinalScale[2]));
		NewMesh.SetImportedTransform(Pos, Rot, Scale);

		// set data to UHMesh class
		NewMesh.SetPositionData(MeshVertices);
		NewMesh.SetUV0Data(MeshUV0);
		NewMesh.SetNormalData(MeshNormal);
		NewMesh.SetTangentData(MeshTangent);

		// at last, try to import material as well
		UniquePtr<UHMaterial> NewMat = ImportMaterial(InNode);
		NewMesh.SetImportedMaterialName(NewMat->GetName());
		if (!UHUtilities::FindByElement(ImportedMaterialNames, NewMat->GetName()))
		{
			ImportedMaterialNames.push_back(NewMat->GetName());
			InMatVector.push_back(std::move(NewMat));
		}

		// export to as UH mesh
		std::string OriginSubpath = UHAssetPath::GetMeshOriginSubpath(InPath);

		NewMesh.Export(GMeshAssetFolder + OriginSubpath);

		// add cache record
		Cache.UHMeshesPath.push_back(GMeshAssetFolder + OriginSubpath + InNode->GetName() + GMeshAssetExtension);
	}
	else
	{
		UHE_LOG(UHUtilities::ToStringW(InNode->GetName()) + L" is not a triangle mesh. This mesh won't be loaded.\n");
	}

	// continue to process child node if there is any
	for (int32_t Idx = 0; Idx < InNode->GetChildCount(); Idx++)
	{
		CreateUHMeshes(InNode->GetChild(Idx), InPath, InMatVector, Cache);
	}
}

void UHFbxImporter::AddRawMeshAssetCache(UHRawMeshAssetCache InAssetCache)
{
	// add non-duplicate cache to list
	if (!UHUtilities::FindByElement(MeshAssetCaches, InAssetCache))
	{
		MeshAssetCaches.push_back(InAssetCache);
	}
}

void UHFbxImporter::LoadCaches()
{
	// check cache folder
	if (!std::filesystem::exists(GMeshAssetCachePath))
	{
		std::filesystem::create_directories(GMeshAssetCachePath);
	}

	for (std::filesystem::recursive_directory_iterator Idx(GMeshAssetCachePath.c_str()), end; Idx != end; Idx++)
	{
		if (std::filesystem::is_directory(Idx->path()) || !UHAssetPath::IsTheSameExtension(Idx->path(), GMeshAssetCacheExtension))
		{
			continue;
		}

		std::ifstream FileIn(Idx->path().string().c_str(), std::ios::in | std::ios::binary);

		// load cache
		UHRawMeshAssetCache Cache;

		// load source path
		std::string TempString;
		UHUtilities::ReadStringData(FileIn, TempString);
		Cache.SourcePath = TempString;

		// load last modified time of source
		FileIn.read(reinterpret_cast<char*>(&Cache.SourcLastModifiedTime), sizeof(Cache.SourcLastModifiedTime));

		// load UHMeshes dependency
		size_t NumAssetDependency;
		FileIn.read(reinterpret_cast<char*>(&NumAssetDependency), sizeof(NumAssetDependency));

		for (size_t Jdx = 0; Jdx < NumAssetDependency; Jdx++)
		{
			UHUtilities::ReadStringData(FileIn, TempString);
			Cache.UHMeshesPath.push_back(TempString);
		}

		FileIn.close();

		AddRawMeshAssetCache(Cache);
	}
}

void UHFbxImporter::OutputCaches()
{
	// check cache folder
	if (!std::filesystem::exists(GMeshAssetCachePath))
	{
		std::filesystem::create_directories(GMeshAssetCachePath);
	}

	// open cache files
	for (const UHRawMeshAssetCache& MeshCache : MeshAssetCaches)
	{
		// find origin path and try to preserve file structure
		std::string OriginSubpath = UHAssetPath::GetMeshOriginSubpath(MeshCache.SourcePath);

		std::string CacheName = MeshCache.SourcePath.stem().string();
		std::ofstream FileOut(GMeshAssetCachePath + OriginSubpath + CacheName + GMeshAssetCacheExtension, std::ios::out | std::ios::binary);

		// output necessary data
		UHUtilities::WriteStringData(FileOut, MeshCache.SourcePath.string());
		FileOut.write(reinterpret_cast<const char*>(&MeshCache.SourcLastModifiedTime), sizeof(&MeshCache.SourcLastModifiedTime));

		size_t NumAssetDependency = MeshCache.UHMeshesPath.size();
		FileOut.write(reinterpret_cast<const char*>(&NumAssetDependency), sizeof(NumAssetDependency));

		for (size_t Jdx = 0; Jdx < NumAssetDependency; Jdx++)
		{
			UHUtilities::WriteStringData(FileOut, MeshCache.UHMeshesPath[Jdx].string());
		}

		FileOut.close();
	}
}

bool UHFbxImporter::IsCached(std::filesystem::path InRawAssetPath)
{
	bool bResult = false;
	for (size_t Idx = 0; Idx < MeshAssetCaches.size() && !bResult; Idx++)
	{
		bool bIsPathEqual = MeshAssetCaches[Idx].SourcePath == InRawAssetPath;
		bool bIsModifiedTimeEqual = MeshAssetCaches[Idx].SourcLastModifiedTime == std::filesystem::last_write_time(InRawAssetPath).time_since_epoch().count();

		bool bAreUHMeshesExist = true;
		for (size_t Jdx = 0; Jdx < MeshAssetCaches[Idx].UHMeshesPath.size(); Jdx++)
		{
			bAreUHMeshesExist &= std::filesystem::exists(MeshAssetCaches[Idx].UHMeshesPath[Jdx]);
		}

		// consider a raw asset is cached if source path, last modified time, and all UHMeshes referenced it are valid
		bResult = bIsPathEqual && bIsModifiedTimeEqual && bAreUHMeshesExist;
	}

	return bResult;
}

#endif