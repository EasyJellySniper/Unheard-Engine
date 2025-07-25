#pragma once
#define NOMINMAX
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include <memory>
#include <fbxsdk.h>
#include <filesystem>	// for list all files under a directory and subdirectory, C++17 feature

class UHMesh;
class UHMaterial;
class UHCameraComponent;
class UHLightBase;
enum class UHLightType;

struct UHFbxCameraData
{
public:
	std::string Name;
	float NearPlane;
	float CullingDistance;
	float Fov;
	XMFLOAT3 Position;
	XMFLOAT3 Rotation;	// pitch yaw roll
};

struct UHFbxLightData
{
public:
	std::string Name;
	UHLightType Type;
	XMFLOAT3 LightColor;
	float Intensity;
	float Radius;
	float SpotAngle;
	XMFLOAT3 Position;
	XMFLOAT3 Rotation;	// pitch yaw roll
};

struct UHFbxImportedData
{
public:
	std::vector<UniquePtr<UHMesh>> ImportedMesh;
	std::vector<UniquePtr<UHMaterial>> ImportedMaterial;
	std::vector<UHFbxCameraData> ImportedCameraData;
	std::vector<UHFbxLightData> ImportedLightData;
};

enum UHFbxConvertUnit
{
	Meter,
	Centimeter
};

class UHFbxImporter
{
public:
	UHFbxImporter();
	~UHFbxImporter();

	void SetConvertUnit(UHFbxConvertUnit InUnit);
	void SetMaterialOutputPath(std::string InPath);

	// this will output a fbx imported data structure
	UHFbxImportedData ImportRawFbx(std::filesystem::path InPath, std::filesystem::path InTextureRefPath);

private:
	// collect all FBX nodes
	void CollectFBXNodes(FbxNode* InNode);

	// import meshes and their materials
	void ImportMeshesAndMaterials(FbxNode* InNode, std::filesystem::path InPath
		, std::filesystem::path InTextureRefPath
		, std::vector<UniquePtr<UHMesh>>& ImportedMesh
		, std::vector<UniquePtr<UHMaterial>>& ImportedMaterial);

	// import cameras/lights, these two will be added to the current scene and not managed by the asset manager
	void ImportCameras(FbxNode* InNode, std::vector<UHFbxCameraData>& ImportedCameraData);
	void ImportLights(FbxNode* InNode, std::vector<UHFbxLightData>& ImportedLightData);

	FbxManager* FbxSDKManager;
	std::vector<std::string> ImportedMaterialNames;
	std::vector<FbxNode*> FbxNodes;
	std::string MaterialOutputPath;

	UHFbxConvertUnit ConvertUnit;
};

#endif