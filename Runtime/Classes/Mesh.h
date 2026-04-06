#pragma once
#define NOMINMAX
#include <vector>
#include <string>
#include <filesystem>
#include "RenderBuffer.h"
#include "Object.h"
#include "../../UnheardEngine.h"
#include "AccelerationStructure.h"
#include "Runtime/Renderer/RenderingTypes.h"

enum class UHMeshVersion
{
	StoreSourcePath = 1,
	MeshVersionMax
};

class UHGraphic;

// Meshlet structure, for now it simply stores the vert/prim count and offset
struct UHMeshlet
{
public:
	UHMeshlet()
		: VertexCount(0)
		, VertexOffset(0)
		, PrimitiveCount(0)
	{

	}

	uint32_t VertexCount;
	uint32_t VertexOffset;
	uint32_t PrimitiveCount;
};

// Mesh class of unheard engine
class UHMesh : public UHObject, public UHRenderState
{
public:
	UHMesh();
	UHMesh(std::string InName);
	void CreateGPUBuffers(UHGraphic* InGfx);
	void CreateBottomLevelAS(UHGraphic* InGfx, VkCommandBuffer InBuffer);
	void ReleaseCPUMeshData();
	void Release();

	void SetPositionData(std::vector<UHVector3> InData);
	void SetUV0Data(std::vector<UHVector2> InData);
	void SetNormalData(std::vector<UHVector3> InData);
	void SetTangentData(std::vector<UHVector4> InData);
	void SetIndicesData(std::vector<uint32_t> InIndicesData);

	std::string GetName() const;
	std::string GetSourcePath() const;
	const std::vector<uint32_t>& GetIndicesData() const;
	const std::vector<uint16_t>& GetIndicesData16() const;

	uint32_t GetVertexCount() const;
	uint32_t GetIndicesCount() const;
	uint32_t GetMeshletCount() const;
	bool IsIndexBufer32Bit() const;

	std::string GetImportedMaterialName() const;
	UHVector3 GetImportedTranslation() const;
	UHVector3 GetImportedRotation() const;
	UHVector3 GetImportedScale() const;
	UHVector3 GetMeshCenter() const;
	UHBoundingBox GetMeshBound() const;

	UHRenderBuffer<UHVector3>* GetPositionBuffer() const;
	UHRenderBuffer<UHVector2>* GetUV0Buffer() const;
	UHRenderBuffer<UHVector3>* GetNormalBuffer() const;
	UHRenderBuffer<UHVector4>* GetTangentBuffer() const;
	UHRenderBuffer<UHMeshlet>* GetMeshletBuffer() const;

	UHRenderBuffer<uint32_t>* GetIndexBuffer() const;
	UHRenderBuffer<uint16_t>* GetIndexBuffer16() const;
	UHAccelerationStructure* GetBottomLevelAS() const;
	int32_t GetHighestIndex() const;

	void RecalculateMeshBound();
	bool Import(std::filesystem::path InUHMeshPath);

#if WITH_EDITOR
	void SetImportedTransform(UHVector3 InTranslation, UHVector3 InRotation, UHVector3 InScale);
	void SetImportedMaterialName(std::string InName);
	void SetSourcePath(const std::string InPath);
	void Export(std::filesystem::path OutputFolder, bool bOverwrite = true);
#endif

	bool operator==(const UHMesh& InMesh);

	// meshlet stuff, a mesh can be divided into multiple meshlets
	// try to keep the MaxVertexPerMeshlet a multiply of 3 of MaxPrimitivePerMeshlet
	static const uint32_t MaxVertexPerMeshlet = 126;
	static const uint32_t MaxPrimitivePerMeshlet = 42;

private:
	void CheckAndConvertToIndices16();
	void CreateMeshlets(UHGraphic* InGfx);

	std::string ImportedMaterialName;
	UHVector3 ImportedTranslation;
	UHVector3 ImportedRotation;
	UHVector3 ImportedScale;
	std::string SourcePath;

	std::vector<UHVector3> PositionData;
	std::vector<UHVector2> UV0Data;
	std::vector<UHVector3> NormalData;
	std::vector<UHVector4> TangentData;

	std::vector<uint32_t> IndicesData;
	std::vector<uint16_t> IndicesData16;
	uint32_t VertexCount;
	uint32_t IndiceCount;

	UHVector3 MeshCenter;
	int32_t HighestIndex;
	bool bIndexBuffer32Bit;
	bool bHasInitialized;

	// GPU VB/IB buffer
	UniquePtr<UHRenderBuffer<UHVector3>> PositionBuffer;
	UniquePtr<UHRenderBuffer<UHVector2>> UV0Buffer;
	UniquePtr<UHRenderBuffer<UHVector3>> NormalBuffer;
	UniquePtr<UHRenderBuffer<UHVector4>> TangentBuffer;

	UniquePtr<UHRenderBuffer<uint32_t>> IndexBuffer;
	UniquePtr<UHRenderBuffer<uint16_t>> IndexBuffer16;
	UniquePtr<UHAccelerationStructure> BottomLevelAS;

	// bound of the mesh
	UHBoundingBox MeshBound;

	uint32_t NumMeshlets;
	std::vector<UHMeshlet> MeshletsData;
	UniquePtr<UHRenderBuffer<UHMeshlet>> MeshletBuffer;
};