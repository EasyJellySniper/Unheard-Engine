#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include "Types.h"
#include "RenderBuffer.h"
#include "MeshLayout.h"
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

	void SetMeshData(std::vector<UHMeshData> InMeshData);
	void SetIndicesData(std::vector<uint32_t> InIndicesData);

	std::string GetName() const;
	std::vector<UHMeshData> GetMeshData() const;
	std::vector<uint32_t> GetIndicesData() const;

	uint32_t GetVertexCount() const;
	uint32_t GetIndicesCount() const;

	std::string GetImportedMaterialName() const;
	XMFLOAT3 GetImportedTranslation() const;
	XMFLOAT3 GetImportedRotation() const;
	XMFLOAT3 GetImportedScale() const;
	XMFLOAT3 GetMeshCenter() const;

	UHRenderBuffer<UHDefaultLitMeshLayout>* GetVertexBuffer() const;
	UHRenderBuffer<uint32_t>* GetIndexBuffer() const;
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

	std::vector<UHMeshData> MeshData;
	std::vector<uint32_t> IndicesData;
	uint32_t VertexCount;
	uint32_t IndiceCount;

	XMFLOAT3 MeshCenter;
	int32_t HighestIndex;

	// GPU VB/IB buffer
	std::unique_ptr<UHRenderBuffer<UHDefaultLitMeshLayout>> VertexBuffer;
	std::unique_ptr<UHRenderBuffer<uint32_t>> IndexBuffer;
	std::unique_ptr<UHAccelerationStructure> BottomLevelAS;
};