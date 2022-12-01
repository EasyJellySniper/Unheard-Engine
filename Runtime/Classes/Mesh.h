#pragma once
#define NOMINMAX
#include <vector>
#include <string>
#include <filesystem>
#include "Types.h"
#include "RenderBuffer.h"
#include "Object.h"
#include "../../UnheardEngine.h"
#include "AccelerationStructure.h"

class UHGraphic;

// Mesh class of unheard engine
class UHMesh : public UHObject
{
public:
	UHMesh();
	UHMesh(std::string InName);
	void CreateGPUBuffers(UHGraphic* InGfx);
	void CreateBottomLevelAS(UHGraphic* InGfx, VkCommandBuffer InBuffer);
	void ReleaseCPUMeshData();
	void Release();

	void SetPositionData(std::vector<XMFLOAT3> InData);
	void SetUV0Data(std::vector<XMFLOAT2> InData);
	void SetNormalData(std::vector<XMFLOAT3> InData);
	void SetTangentData(std::vector<XMFLOAT4> InData);
	void SetIndicesData(std::vector<uint32_t> InIndicesData);

	std::string GetName() const;
	std::vector<uint32_t> GetIndicesData() const;
	std::vector<uint16_t> GetIndicesData16() const;

	uint32_t GetVertexCount() const;
	uint32_t GetIndicesCount() const;
	bool IsIndexBufer32Bit() const;

	std::string GetImportedMaterialName() const;
	XMFLOAT3 GetImportedTranslation() const;
	XMFLOAT3 GetImportedRotation() const;
	XMFLOAT3 GetImportedScale() const;
	XMFLOAT3 GetMeshCenter() const;
	BoundingBox GetMeshBound() const;

	UHRenderBuffer<XMFLOAT3>* GetPositionBuffer() const;
	UHRenderBuffer<XMFLOAT2>* GetUV0Buffer() const;
	UHRenderBuffer<XMFLOAT3>* GetNormalBuffer() const;
	UHRenderBuffer<XMFLOAT4>* GetTangentBuffer() const;

	UHRenderBuffer<uint32_t>* GetIndexBuffer() const;
	UHRenderBuffer<uint16_t>* GetIndexBuffer16() const;
	UHAccelerationStructure* GetBottomLevelAS() const;
	int32_t GetHighestIndex() const;

	bool Import(std::filesystem::path InUHMeshPath);

#if WITH_DEBUG
	void SetImportedTransform(XMFLOAT3 InTranslation, XMFLOAT3 InRotation, XMFLOAT3 InScale);
	void SetImportedMaterialName(std::string InName);
	void ApplyUnitScale();
	void Export(std::filesystem::path OutputFolder, bool bOverwrite = true);
#endif

private:
	std::string Name;

	std::string ImportedMaterialName;
	XMFLOAT3 ImportedTranslation;
	XMFLOAT3 ImportedRotation;
	XMFLOAT3 ImportedScale;

	std::vector<XMFLOAT3> PositionData;
	std::vector<XMFLOAT2> UV0Data;
	std::vector<XMFLOAT3> NormalData;
	std::vector<XMFLOAT4> TangentData;

	std::vector<uint32_t> IndicesData;
	std::vector<uint16_t> IndicesData16;
	uint32_t VertexCount;
	uint32_t IndiceCount;

	XMFLOAT3 MeshCenter;
	int32_t HighestIndex;
	bool bIndexBuffer32Bit;

	// GPU VB/IB buffer
	std::unique_ptr<UHRenderBuffer<XMFLOAT3>> PositionBuffer;
	std::unique_ptr<UHRenderBuffer<XMFLOAT2>> UV0Buffer;
	std::unique_ptr<UHRenderBuffer<XMFLOAT3>> NormalBuffer;
	std::unique_ptr<UHRenderBuffer<XMFLOAT4>> TangentBuffer;

	std::unique_ptr<UHRenderBuffer<uint32_t>> IndexBuffer;
	std::unique_ptr<UHRenderBuffer<uint16_t>> IndexBuffer16;
	std::unique_ptr<UHAccelerationStructure> BottomLevelAS;

	// bound of the mesh
	BoundingBox MeshBound;
};