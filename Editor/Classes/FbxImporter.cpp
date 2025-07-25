#include "FbxImporter.h"

#if WITH_EDITOR
#include <fstream>
#include "Runtime/Classes/Utility.h"
#include "Runtime/Classes/AssetPath.h"
#include "Runtime/Classes/Mesh.h"
#include "Runtime/Classes/Material.h"
#include "Runtime/Components/Camera.h"
#include "Runtime/Components/Light.h"

UHFbxImporter::UHFbxImporter()
	: ConvertUnit(UHFbxConvertUnit::Meter)
{
	// create fbx manager
	FbxSDKManager = FbxManager::Create();
}

UHFbxImporter::~UHFbxImporter()
{
	// destroy fbx manager
	FbxSDKManager->Destroy();
	FbxSDKManager = nullptr;
}

void UHFbxImporter::SetConvertUnit(UHFbxConvertUnit InUnit)
{
	ConvertUnit = InUnit;
}

void UHFbxImporter::SetMaterialOutputPath(std::string InPath)
{
	MaterialOutputPath = InPath;
}

bool IsNodeAttributeType(const FbxNode* InNode, FbxNodeAttribute::EType InAttributeType)
{
	if (InNode == nullptr)
	{
		return false;
	}

	for (int32_t Idx = 0; Idx < InNode->GetNodeAttributeCount(); Idx++)
	{
		if (InNode->GetNodeAttributeByIndex(Idx)->GetAttributeType() == InAttributeType)
		{
			return true;
		}
	}

	return false;
}

UHFbxImportedData UHFbxImporter::ImportRawFbx(std::filesystem::path InPath, std::filesystem::path InTextureRefPath)
{
	UHFbxImportedData OutData{};

	// IO settings initialization
	FbxIOSettings* FbxSDKIOSettings = FbxIOSettings::Create(FbxSDKManager, IOSROOT);
	FbxSDKManager->SetIOSettings(FbxSDKIOSettings);
	ImportedMaterialNames.clear();
	FbxNodes.clear();

	// importer initialization, create fbx importer
	FbxImporter* FbxSDKImporter = FbxImporter::Create(FbxSDKManager, "UHFbxImporter");
	if (!FbxSDKImporter->Initialize(InPath.string().c_str(), -1, FbxSDKManager->GetIOSettings()))
	{
		// it could be here if non-fbx file was found
		UHE_LOG(L"Failed to load " + InPath.wstring() + L"\n");
		return OutData;
	}

	// Import the contents of the file into the scene.
	FbxScene* Scene = FbxScene::Create(FbxSDKManager, "UHFbxScene");
	if (FbxSDKImporter->Import(Scene))
	{
		if (ConvertUnit == UHFbxConvertUnit::Meter)
		{
			// force as meter
			FbxSystemUnit::m.ConvertScene(Scene);
		}
		else if (ConvertUnit == UHFbxConvertUnit::Centimeter)
		{
			// force as cm
			FbxSystemUnit::cm.ConvertScene(Scene);
		}

		// force convert to the axis system I need
		FbxAxisSystem UHAxisSystem;
		if (FbxAxisSystem::ParseAxisSystem("xyz", UHAxisSystem))
		{
			UHAxisSystem.DeepConvertScene(Scene);
		}

		// force triangle meshes
		FbxGeometryConverter GeoConverter(FbxSDKManager);
		GeoConverter.Triangulate(Scene, true);

		// collect ALL fbx nodes as a vector for easy access later
		CollectFBXNodes(Scene->GetRootNode());

		// import fbx data based on the node type
		for (size_t NodeIdx = 0; NodeIdx < FbxNodes.size(); NodeIdx++)
		{
			FbxNode* Node = FbxNodes[NodeIdx];
			if (Node == nullptr)
			{
				continue;
			}

			if (IsNodeAttributeType(Node, FbxNodeAttribute::eMesh))
			{
				ImportMeshesAndMaterials(Node, InPath, InTextureRefPath, OutData.ImportedMesh, OutData.ImportedMaterial);
			}
			else if (IsNodeAttributeType(Node, FbxNodeAttribute::eCamera))
			{
				ImportCameras(Node, OutData.ImportedCameraData);
			}
			else if (IsNodeAttributeType(Node, FbxNodeAttribute::eLight))
			{
				ImportLights(Node, OutData.ImportedLightData);
			}
		}
	}

	FbxSDKImporter->Destroy();
	Scene->Destroy();

	// finish importing, destroy them
	FbxSDKIOSettings->Destroy();
	FbxSDKIOSettings = nullptr;
	ImportedMaterialNames.clear();
	FbxNodes.clear();

	return OutData;
}

void UHFbxImporter::CollectFBXNodes(FbxNode* InNode)
{
	if (InNode == nullptr)
	{
		return;
	}

	// recursively collecting nodes
	FbxNodes.push_back(InNode);
	for (int32_t Idx = 0; Idx < InNode->GetChildCount(); Idx++)
	{
		CollectFBXNodes(InNode->GetChild(Idx));
	}
}

void ExtractTexturesFromFbx(FbxProperty Property, UHMaterial* UHMat, std::filesystem::path InTextureRefPath)
{
	int32_t TextureCount = Property.GetSrcObjectCount<FbxTexture>();
	for (int32_t Idx = 0; Idx < TextureCount; Idx++)
	{
		// skip layred texture for now
		FbxLayeredTexture* LayeredTexture = Property.GetSrcObject<FbxLayeredTexture>(Idx);
		if (LayeredTexture)
		{
			continue;
		}

		// get regular texture info
		FbxTexture* Texture = Property.GetSrcObject<FbxTexture>(Idx);
		if (Texture)
		{
			// get texture type and filename
			std::string TexType = Property.GetNameAsCStr();
			FbxFileTexture* FileTexture = FbxCast<FbxFileTexture>(Texture);
			std::filesystem::path TexFileName = FileTexture->GetFileName();
			TexFileName = InTextureRefPath.string() + "/" + TexFileName.stem().string();
			TexFileName = std::filesystem::relative(TexFileName, GTextureAssetFolder);
			std::string TexFileNameString = TexFileName.string();
			TexFileNameString = UHUtilities::StringReplace(TexFileNameString, "\\", "/");

			if (TexType == FbxSurfaceMaterial::sDiffuse)
			{
				UHMat->SetTexFileName(UHMaterialInputs::Diffuse, TexFileNameString);
			}
			else if (TexType == FbxSurfaceMaterial::sDiffuseFactor)
			{
				// treat diffuse factor map as AO map
				UHMat->SetTexFileName(UHMaterialInputs::Occlusion, TexFileNameString);
			}
			else if (TexType == FbxSurfaceMaterial::sSpecular 
				|| TexType == FbxSurfaceMaterial::sReflectionFactor
				|| TexType == FbxSurfaceMaterial::sSpecularFactor)
			{
				UHMat->SetTexFileName(UHMaterialInputs::Specular, TexFileNameString);
			}
			else if (TexType == FbxSurfaceMaterial::sNormalMap || TexType == FbxSurfaceMaterial::sBump)
			{
				// old Bump map (gray) will be used as normal in UH
				// always ask artists use Normal map
				UHMat->SetTexFileName(UHMaterialInputs::Normal, TexFileNameString);
			}
			else if (TexType == FbxSurfaceMaterial::sTransparentColor || TexType == FbxSurfaceMaterial::sTransparencyFactor)
			{
				UHMat->SetTexFileName(UHMaterialInputs::Opacity, TexFileNameString);

				// default to masked object if it contains opacity texture
				UHMat->SetBlendMode(UHBlendMode::Masked);
			}
			else if (TexType == FbxSurfaceMaterial::sShininess)
			{
				// roughness texture setting
				UHMat->SetTexFileName(UHMaterialInputs::Roughness, TexFileNameString);
			}
			else
			{
				UHE_LOG(L"Unsupported texture type detected: " + UHUtilities::ToStringW(TexType) + L". This one will be skipped.\n");
			}
		}
	}
}

// mirror from FBX example: https://help.autodesk.com/view/FBX/2016/ENU/?guid=__cpp_ref__import_scene_2_display_material_8cxx_example_html
UniquePtr<UHMaterial> ImportMaterial(FbxNode* InNode, std::filesystem::path InTextureRefPath)
{
	UniquePtr<UHMaterial> UHMat = MakeUnique<UHMaterial>();
	int32_t MatCount = InNode->GetMaterialCount();

	// only import single material per mesh at the moment
	if (MatCount > 0)
	{
		FbxPropertyT<FbxDouble3> FbxDoubleVec3;
		FbxPropertyT<FbxDouble> FbxDouble1;
		FbxColor Color;

		FbxSurfaceMaterial* Mat = InNode->GetMaterial(0);
		std::wstring MatName = UHUtilities::ToStringW(Mat->GetName());
		UHMat->SetName(Mat->GetName());

		// extract matarial props that are suitable for UH
		UHMaterialProperty MatProps{};

		if (Mat->GetClassId().Is(FbxSurfacePhong::ClassId))
		{
			// diffuse
			FbxDoubleVec3 = ((FbxSurfacePhong*)Mat)->Diffuse;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->DiffuseFactor;
			MatProps.Diffuse.x = static_cast<float>(FbxDoubleVec3.Get()[0] * FbxDouble1);
			MatProps.Diffuse.y = static_cast<float>(FbxDoubleVec3.Get()[1] * FbxDouble1);
			MatProps.Diffuse.z = static_cast<float>(FbxDoubleVec3.Get()[2] * FbxDouble1);

			// opacity
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->TransparencyFactor;
			MatProps.Opacity = static_cast<float>(FbxDouble1.Get());

			// emissive
			FbxDoubleVec3 = ((FbxSurfacePhong*)Mat)->Emissive;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->EmissiveFactor;
			MatProps.Emissive.x = static_cast<float>(FbxDoubleVec3.Get()[0] * FbxDouble1);
			MatProps.Emissive.y = static_cast<float>(FbxDoubleVec3.Get()[1] * FbxDouble1);
			MatProps.Emissive.z = static_cast<float>(FbxDoubleVec3.Get()[2] * FbxDouble1);

			// roughness
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->Shininess;
			MatProps.Roughness = 1.0f - static_cast<float>(FbxDouble1.Get() / 255.0f);

			// specular color
			FbxDoubleVec3 = ((FbxSurfacePhong*)Mat)->Specular;
			MatProps.Specular.x = static_cast<float>(FbxDoubleVec3.Get()[0]);
			MatProps.Specular.y = static_cast<float>(FbxDoubleVec3.Get()[1]);
			MatProps.Specular.z = static_cast<float>(FbxDoubleVec3.Get()[2]);

			// bump scale
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->BumpFactor;
			MatProps.BumpScale = static_cast<float>(FbxDouble1.Get());
		}
		else if (Mat->GetClassId().Is(FbxSurfaceLambert::ClassId))
		{
			// diffuse
			FbxDoubleVec3 = ((FbxSurfacePhong*)Mat)->Diffuse;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->DiffuseFactor;
			MatProps.Diffuse.x = static_cast<float>(FbxDoubleVec3.Get()[0] * FbxDouble1);
			MatProps.Diffuse.y = static_cast<float>(FbxDoubleVec3.Get()[1] * FbxDouble1);
			MatProps.Diffuse.z = static_cast<float>(FbxDoubleVec3.Get()[2] * FbxDouble1);

			// opacity
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->TransparencyFactor;
			MatProps.Opacity = static_cast<float>(FbxDouble1.Get());

			// emissive
			FbxDoubleVec3 = ((FbxSurfacePhong*)Mat)->Emissive;
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->EmissiveFactor;
			MatProps.Emissive.x = static_cast<float>(FbxDoubleVec3.Get()[0] * FbxDouble1);
			MatProps.Emissive.y = static_cast<float>(FbxDoubleVec3.Get()[1] * FbxDouble1);
			MatProps.Emissive.z = static_cast<float>(FbxDoubleVec3.Get()[2] * FbxDouble1);

			// bump scale
			FbxDouble1 = ((FbxSurfacePhong*)Mat)->BumpFactor;
			MatProps.BumpScale = static_cast<float>(FbxDouble1.Get());
		}
		else
		{
			// weird case, it is a FbxSurfaceMaterial but not using phone/lambert at all?
			// get whatever it can provide to us
			FbxProperty FbxProp{};

			// diffuse
			FbxProp = Mat->FindProperty(Mat->sDiffuse);
			if (FbxProp.IsValid())
			{
				FbxDoubleVec3 = FbxProp;
				MatProps.Diffuse.x = static_cast<float>(FbxDoubleVec3.Get()[0]);
				MatProps.Diffuse.y = static_cast<float>(FbxDoubleVec3.Get()[1]);
				MatProps.Diffuse.z = static_cast<float>(FbxDoubleVec3.Get()[2]);
			}

			// opacity
			FbxProp = Mat->FindProperty(Mat->sTransparencyFactor);
			if (FbxProp.IsValid())
			{
				FbxDouble1 = FbxProp;
				MatProps.Opacity = static_cast<float>(FbxDouble1.Get());
			}

			// emissive
			FbxProp = Mat->FindProperty(Mat->sEmissive);
			if (FbxProp.IsValid())
			{
				FbxDoubleVec3 = FbxProp;
				MatProps.Emissive.x = static_cast<float>(FbxDoubleVec3.Get()[0]);
				MatProps.Emissive.y = static_cast<float>(FbxDoubleVec3.Get()[1]);
				MatProps.Emissive.z = static_cast<float>(FbxDoubleVec3.Get()[2]);
			}

			// roughness
			FbxProp = Mat->FindProperty(Mat->sShininess);
			if (FbxProp.IsValid())
			{
				FbxDouble1 = FbxProp;
				MatProps.Roughness = 1.0f - static_cast<float>(FbxDouble1.Get() / 255.0f);
				MatProps.Roughness = std::clamp(MatProps.Roughness, 0.0f, 1.0f);
			}

			// specular
			FbxProp = Mat->FindProperty(Mat->sSpecular);
			if (FbxProp.IsValid())
			{
				FbxDoubleVec3 = FbxProp;
				MatProps.Specular.x = static_cast<float>(FbxDoubleVec3.Get()[0]);
				MatProps.Specular.y = static_cast<float>(FbxDoubleVec3.Get()[1]);
				MatProps.Specular.z = static_cast<float>(FbxDoubleVec3.Get()[2]);
			}

			// bump factor
			FbxProp = Mat->FindProperty(Mat->sBumpFactor);
			if (FbxProp.IsValid())
			{
				FbxDouble1 = FbxProp;
				MatProps.BumpScale = static_cast<float>(FbxDouble1.Get());
			}
		}

		if (MatCount > 1)
		{
			UHE_LOG(L"Multiple materials detect: " + MatName + L". Only the first material will be used for now.\n");
		}

		UHMat->SetMaterialProps(MatProps);

		// iterate textures, https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/_import_scene_2_display_texture_8cxx-example.html
		int32_t TextureIndex;
		FBXSDK_FOR_EACH_TEXTURE(TextureIndex)
		{
			FbxProperty Property = Mat->FindProperty(FbxLayerElement::sTextureChannelNames[TextureIndex]);
			if (Property.IsValid())
			{
				ExtractTexturesFromFbx(Property, UHMat.get(), InTextureRefPath);
			}
		}
	}

	return UHMat;
}

// reference: https://help.autodesk.com/cloudhelp/2018/ENU/FBX-Developer-Help/cpp_ref/_import_scene_2_display_mesh_8cxx-example.html
void UHFbxImporter::ImportMeshesAndMaterials(FbxNode* InNode, std::filesystem::path InPath
	, std::filesystem::path InTextureRefPath
	, std::vector<UniquePtr<UHMesh>>& ImportedMesh
	, std::vector<UniquePtr<UHMaterial>>& ImportedMaterial)
{
	// model loading here is straight forward, I only load what I need at the moment
	// if normal/tangent data is missing, I'd rely on Fbx's generation only
	// vertices and indices are loaded separaterly, so GetPolygonVertices() is the only polygon usage here
	// this fits graphic API's vertex/index buffer concept well
	// the caller should check whether it's a mesh node
	if (InNode == nullptr || InNode->GetMesh()->GetControlPointsCount() == 0)
	{
		return;
	}

	std::wstring NodeNameW = UHUtilities::ToStringW(InNode->GetName());
	FbxMesh* InMesh = InNode->GetMesh();

	if (InMesh->GetTextureUVCount() == 0)
	{
		UHE_LOG(L"Fbx without texture UV found, this mesh will be ignored: " + NodeNameW + L"\n");
		return;
	}

	if (InMesh->GetElementNormal() == nullptr)
	{
		InMesh->GenerateNormals(true);
	}

	if (InMesh->GetElementTangent() == nullptr)
	{
		InMesh->GenerateTangentsData(0, true);
	}

	FbxAMatrix FinalMatrix = InNode->EvaluateGlobalTransform();
	FbxVector4 FinalPos = FinalMatrix.GetT();
	FbxVector4 FinalRot = FinalMatrix.GetR();
	FbxVector4 FinalScale = FinalMatrix.GetS();

	// only create UH mesh if it's mesh node and valid transform
	UniquePtr<UHMesh> NewMesh = MakeUnique<UHMesh>(InNode->GetName());
	const bool bNeedDuplication = InMesh->GetTextureUVCount() >= InMesh->GetControlPointsCount();
	const int32_t OutputVertexCount = bNeedDuplication ? InMesh->GetTextureUVCount() : InMesh->GetControlPointsCount();

	// start adding mesh infors
	std::vector<uint32_t> MeshIndices;
	std::vector<XMFLOAT3> MeshVertices(OutputVertexCount);
	std::vector<XMFLOAT2> MeshUVs(OutputVertexCount);
	std::vector<XMFLOAT3> MeshNormals(OutputVertexCount);
	std::vector<XMFLOAT4> MeshTangents(OutputVertexCount);

	// get source vertices
	FbxVector4* MeshPos = InMesh->GetControlPoints();

	// iterate all triangles and load vertices/indices/UVs/Normals/Tangents at once
	// the code below will also reconstruct the triangles based on the UV indices
	// as there is a chance that a fbx mesh has higher UV counts than vertex counts
	// in that case, I need to duplicate triangles for the engine use

	// accumulate the vertex id for normal/tangent lookup
	// not sure why FBX SDK uses two different ways in their sample code!
	int32_t VertexId = 0;
	for (int32_t Idx = 0; Idx < InMesh->GetPolygonCount(); Idx++)
	{
		const FbxGeometryElementUV* UVElement = InMesh->GetElementUV();
		int32_t UVIndex = UHINDEXNONE;

		for (int32_t Jdx = 0; Jdx < 3; Jdx++)
		{
			// index to control point (original vertices)
			const int32_t ControlIndex = InMesh->GetPolygonVertex(Idx, Jdx);

			// UV index setup
			if (UVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
			{
				UVIndex = UVElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
					? UVElement->GetIndexArray().GetAt(VertexId)
					: VertexId;
			}
			else if (UVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
			{
				UVIndex = UVElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
					? UVElement->GetIndexArray().GetAt(ControlIndex)
					: ControlIndex;
			}

			const int32_t OutputIndex = bNeedDuplication ? UVIndex : ControlIndex;

			// indices
			MeshIndices.push_back(OutputIndex);

			// vertices
			{
				XMFLOAT3 Pos;
				Pos.x = static_cast<float>(MeshPos[ControlIndex][0]);
				Pos.y = static_cast<float>(MeshPos[ControlIndex][1]);
				Pos.z = static_cast<float>(MeshPos[ControlIndex][2]);
				MeshVertices[OutputIndex] = Pos;
			}

			// UVs
			{
				FbxVector2 UVValue = UVElement->GetDirectArray().GetAt(UVIndex);
				MeshUVs[OutputIndex].x = static_cast<float>(UVValue[0]);
				MeshUVs[OutputIndex].y = 1.0f - static_cast<float>(UVValue[1]);
			}

			// Normals
			{
				const FbxGeometryElementNormal* NormalElement = InMesh->GetElementNormal();
				int32_t NormalIndex = UHINDEXNONE;

				if (NormalElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
				{
					NormalIndex = NormalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
						? NormalElement->GetIndexArray().GetAt(VertexId)
						: VertexId;
				}
				else if (NormalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
				{
					NormalIndex = NormalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
						? NormalElement->GetIndexArray().GetAt(ControlIndex)
						: ControlIndex;
				}

				FbxVector4 NormalValue = NormalElement->GetDirectArray().GetAt(NormalIndex);
				MeshNormals[OutputIndex].x = static_cast<float>(NormalValue[0]);
				MeshNormals[OutputIndex].y = static_cast<float>(NormalValue[1]);
				MeshNormals[OutputIndex].z = static_cast<float>(NormalValue[2]);
			}

			// Tangents
			{
				const FbxGeometryElementTangent* TangentElement = InMesh->GetElementTangent();
				int32_t TangentIndex = UHINDEXNONE;

				if (TangentElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
				{
					TangentIndex = TangentElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
						? TangentElement->GetIndexArray().GetAt(VertexId)
						: VertexId;
				}
				else if (TangentElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
				{
					TangentIndex = TangentElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect
						? TangentElement->GetIndexArray().GetAt(ControlIndex)
						: ControlIndex;
				}

				FbxVector4 TangentValue = TangentElement->GetDirectArray().GetAt(TangentIndex);
				MeshTangents[OutputIndex].x = static_cast<float>(TangentValue[0]);
				MeshTangents[OutputIndex].y = static_cast<float>(TangentValue[1]);
				MeshTangents[OutputIndex].z = static_cast<float>(TangentValue[2]);
				MeshTangents[OutputIndex].w = static_cast<float>(TangentValue[3]);
			}

			VertexId++;
		}
	}

	NewMesh->SetIndicesData(MeshIndices);

	// get normal, and ensure it's not zero vector
	for (XMFLOAT3& N : MeshNormals)
	{
		if (MathHelpers::IsVectorNearlyZero(N))
		{
			N = XMFLOAT3(0.0001f, 0.0001f, 0.0001f);
		}
	}

	// get tangent
	for (XMFLOAT4& T : MeshTangents)
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
	NewMesh->SetImportedTransform(Pos, Rot, Scale);

	// set data to UHMesh class
	NewMesh->SetPositionData(MeshVertices);
	NewMesh->SetUV0Data(MeshUVs);
	NewMesh->SetNormalData(MeshNormals);
	NewMesh->SetTangentData(MeshTangents);
	NewMesh->RecalculateMeshBound();

	// at last, try to import material as well
	UniquePtr<UHMaterial> NewMat = ImportMaterial(InNode, InTextureRefPath);

	// setup source path of materials with ./ prefix removed
	std::filesystem::path OutPath = MaterialOutputPath + "/" + NewMat->GetName() + GMaterialAssetExtension;
	std::filesystem::path SourcePath = std::filesystem::relative(MaterialOutputPath, GMaterialAssetPath).string() + "/" + NewMat->GetName();
	NewMat->SetSourcePath(UHUtilities::StringReplace(SourcePath.string(), "./", ""));
	NewMesh->SetImportedMaterialName(NewMat->GetSourcePath());

	if (!UHUtilities::FindByElement(ImportedMaterialNames, NewMat->GetSourcePath()))
	{
		ImportedMaterialNames.push_back(NewMat->GetSourcePath());
		ImportedMaterial.push_back(std::move(NewMat));
	}

	ImportedMesh.push_back(std::move(NewMesh));
}

void UHFbxImporter::ImportCameras(FbxNode* InNode, std::vector<UHFbxCameraData>& ImportedCameraData)
{
	if (InNode == nullptr)
	{
		return;
	}

	// create and set camera properties
	FbxCamera* InCamera = InNode->GetCamera();

	UHFbxCameraData NewCameraData;
	NewCameraData.Name = InCamera->GetName();
	NewCameraData.NearPlane = static_cast<float>(InCamera->GetNearPlane());
	NewCameraData.CullingDistance = static_cast<float>(InCamera->GetFarPlane());
	NewCameraData.Fov = static_cast<float>(InCamera->FieldOfView.Get());

	// camera transform
	FbxAMatrix CameraMatrix = InNode->EvaluateGlobalTransform();
	FbxVector4 CameraPos = CameraMatrix.GetT();
	FbxVector4 CameraRot = CameraMatrix.GetR();

	NewCameraData.Position = XMFLOAT3(
		static_cast<float>(CameraPos[0])
		, static_cast<float>(CameraPos[1])
		, static_cast<float>(CameraPos[2]));

	// discard camera pitch and roll for good
	NewCameraData.Rotation = XMFLOAT3(
		0.0f
		, static_cast<float>(CameraRot[1])
		, 0.0f);

	ImportedCameraData.push_back(NewCameraData);
}

void UHFbxImporter::ImportLights(FbxNode* InNode, std::vector<UHFbxLightData>& ImportedLightData)
{
	if (InNode == nullptr)
	{
		return;
	}

	FbxLight* InLight = InNode->GetLight();
	FbxDouble3 LightColor = InLight->Color.Get();
	FbxDouble LightIntensity = InLight->Intensity.Get();

	// light transform
	FbxAMatrix LightMatrix = InNode->EvaluateGlobalTransform();
	FbxVector4 LightPos = LightMatrix.GetT();
	FbxVector4 LightRot = LightMatrix.GetR();

	UHFbxLightData NewLightData;
	NewLightData.Name = InLight->GetName();
	if (InLight->LightType.Get() == FbxLight::EType::eDirectional)
	{
		NewLightData.Type = UHLightType::Directional;
	}
	else if (InLight->LightType.Get() == FbxLight::EType::ePoint)
	{
		NewLightData.Type = UHLightType::Point;
	}
	else if (InLight->LightType.Get() == FbxLight::EType::eSpot)
	{
		NewLightData.Type = UHLightType::Spot;
	}
	else
	{
		UHE_LOG(L"Unsupported light type detected (area or volume). This light will be ignored for now.");
		return;
	}

	NewLightData.LightColor = XMFLOAT3(
		static_cast<float>(LightColor[0]),
		static_cast<float>(LightColor[1]),
		static_cast<float>(LightColor[2])
	);

	// at least give a default intensity if it's too small
	NewLightData.Intensity = std::max(static_cast<float>(LightIntensity / 10000.0f), 5.0f);
	NewLightData.Radius = std::max(static_cast<float>(InLight->FarAttenuationEnd.Get()), 10.0f);
	NewLightData.SpotAngle = static_cast<float>(InLight->OuterAngle.Get());

	NewLightData.Position = XMFLOAT3(
		static_cast<float>(LightPos[0])
		, static_cast<float>(LightPos[1])
		, static_cast<float>(LightPos[2]));

	NewLightData.Rotation = XMFLOAT3(
		static_cast<float>(LightRot[0])
		, static_cast<float>(LightRot[1])
		, static_cast<float>(LightRot[2]));

	ImportedLightData.push_back(NewLightData);
}

#endif