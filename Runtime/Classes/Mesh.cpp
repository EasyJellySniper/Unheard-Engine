#include "Mesh.h"
#include <fstream>
#include "Utility.h"
#include "../Classes/AssetPath.h"
#include "../Engine/Graphic.h"
#include "../CoreGlobals.h"

UHMesh::UHMesh()
	: UHMesh("")
{

}

UHMesh::UHMesh(std::string InName)
	: ImportedTranslation(0.0f, 0.0f, 0.0f)
	, ImportedRotation(0.0f, 0.0f, 0.0f)
	, ImportedScale(1.0f, 1.0f, 1.0f)
	, Name(InName)
	, ImportedMaterialName("NONE")
	, VertexCount(0)
	, IndiceCount(0)
	, MeshCenter(XMFLOAT3(0, 0, 0))
	, PositionBuffer(nullptr)
	, UV0Buffer(nullptr)
	, NormalBuffer(nullptr)
	, TangentBuffer(nullptr)
	, IndexBuffer(nullptr)
	, IndexBuffer16(nullptr)
	, BottomLevelAS(nullptr)
	, HighestIndex(-1)
	, bIndexBuffer32Bit(false)
{

}

// call this function to build gpu buffer
void UHMesh::CreateGPUBuffers(UHGraphic* InGfx)
{
	// prevent duplicate creation
	if (PositionBuffer != nullptr 
		&& (IndexBuffer != nullptr || IndexBuffer16 != nullptr)
		&& UV0Buffer != nullptr
		&& NormalBuffer != nullptr
		&& TangentBuffer != nullptr)
	{
		return;
	}

	PositionBuffer = InGfx->RequestRenderBuffer<XMFLOAT3>();
	UV0Buffer = InGfx->RequestRenderBuffer<XMFLOAT2>();
	NormalBuffer = InGfx->RequestRenderBuffer<XMFLOAT3>();
	TangentBuffer = InGfx->RequestRenderBuffer<XMFLOAT4>();

	// consider 32 or 16 bit index buffer
	if (bIndexBuffer32Bit)
	{
		IndexBuffer = InGfx->RequestRenderBuffer<uint32_t>();
	}
	else
	{
		IndexBuffer16 = InGfx->RequestRenderBuffer<uint16_t>();
	}

	// create mesh buffer
	// VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is necessary for buffer address access
	VkBufferUsageFlags VBFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VkBufferUsageFlags IBFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if (GEnableRayTracing)
	{
		VBFlags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		IBFlags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	bool bValid = true;
	bValid &= PositionBuffer->CreaetBuffer(VertexCount, VBFlags, GGPUMeshBufferMemory.get());
	bValid &= UV0Buffer->CreaetBuffer(VertexCount, VBFlags, GGPUMeshBufferMemory.get());
	bValid &= NormalBuffer->CreaetBuffer(VertexCount, VBFlags, GGPUMeshBufferMemory.get());
	bValid &= TangentBuffer->CreaetBuffer(VertexCount, VBFlags, GGPUMeshBufferMemory.get());

	if (bIndexBuffer32Bit)
	{
		bValid &= IndexBuffer->CreaetBuffer(GetIndicesCount(), IBFlags, GGPUMeshBufferMemory.get());
	}
	else
	{
		bValid &= IndexBuffer16->CreaetBuffer(GetIndicesCount(), IBFlags, GGPUMeshBufferMemory.get());
	}

	// don't upload to GPU if buffer is failed during initialization
	if (!bValid)
	{
		return;
	}

	// upload vb/ib data
	PositionBuffer->UploadAllDataShared(PositionData.data(), GGPUMeshBufferMemory.get());
	UV0Buffer->UploadAllDataShared(UV0Data.data(), GGPUMeshBufferMemory.get());
	NormalBuffer->UploadAllDataShared(NormalData.data(), GGPUMeshBufferMemory.get());
	TangentBuffer->UploadAllDataShared(TangentData.data(), GGPUMeshBufferMemory.get());

	if (bIndexBuffer32Bit)
	{
		IndexBuffer->UploadAllDataShared(GetIndicesData().data(), GGPUMeshBufferMemory.get());
	}
	else
	{
		IndexBuffer16->UploadAllDataShared(GetIndicesData16().data(), GGPUMeshBufferMemory.get());
	}
}

// create bottom level AS for the mesh
void UHMesh::CreateBottomLevelAS(UHGraphic* InGfx, VkCommandBuffer InBuffer)
{
	if (BottomLevelAS == nullptr)
	{
		// ensure mesh data is already created
		CreateGPUBuffers(InGfx);
		BottomLevelAS = InGfx->RequestAccelerationStructure();
		BottomLevelAS->CreaetBottomAS(this, InBuffer);
	}
}

// release cpu data
// we can save memory after uploading to GPU
void UHMesh::ReleaseCPUMeshData()
{
	PositionData.clear();
	UV0Data.clear();
	NormalData.clear();
	TangentData.clear();

	IndicesData.clear();
	IndicesData16.clear();
}

void UHMesh::Release()
{
	UH_SAFE_RELEASE(PositionBuffer);
	PositionBuffer.reset();

	UH_SAFE_RELEASE(UV0Buffer);
	UV0Buffer.reset();

	UH_SAFE_RELEASE(NormalBuffer);
	NormalBuffer.reset();

	UH_SAFE_RELEASE(TangentBuffer);
	TangentBuffer.reset();

	UH_SAFE_RELEASE(IndexBuffer);
	IndexBuffer.reset();

	UH_SAFE_RELEASE(IndexBuffer16);
	IndexBuffer16.reset();

	UH_SAFE_RELEASE(BottomLevelAS);
	BottomLevelAS.reset();
}

void UHMesh::SetPositionData(std::vector<XMFLOAT3> InData)
{
	PositionData = InData;
}

void UHMesh::SetUV0Data(std::vector<XMFLOAT2> InData)
{
	UV0Data = InData;
}

void UHMesh::SetNormalData(std::vector<XMFLOAT3> InData)
{
	NormalData = InData;
}

void UHMesh::SetTangentData(std::vector<XMFLOAT4> InData)
{
	TangentData = InData;
}

void UHMesh::SetIndicesData(std::vector<uint32_t> InIndicesData)
{
	IndicesData = InIndicesData;
}

std::string UHMesh::GetName() const
{
	return Name;
}

std::vector<uint32_t> UHMesh::GetIndicesData() const
{
	return IndicesData;
}

std::vector<uint16_t> UHMesh::GetIndicesData16() const
{
	return IndicesData16;
}

uint32_t UHMesh::GetVertexCount() const
{
	return VertexCount;
}

uint32_t UHMesh::GetIndicesCount() const
{
	return IndiceCount;
}

bool UHMesh::IsIndexBufer32Bit() const
{
	return bIndexBuffer32Bit;
}

std::string UHMesh::GetImportedMaterialName() const
{
	return ImportedMaterialName;
}

XMFLOAT3 UHMesh::GetImportedTranslation() const
{
	return ImportedTranslation;
}

XMFLOAT3 UHMesh::GetImportedRotation() const
{
	return ImportedRotation;
}

XMFLOAT3 UHMesh::GetImportedScale() const
{
	return ImportedScale;
}

XMFLOAT3 UHMesh::GetMeshCenter() const
{
	return MeshCenter;
}

UHRenderBuffer<XMFLOAT3>* UHMesh::GetPositionBuffer() const
{
	return PositionBuffer.get();
}

UHRenderBuffer<XMFLOAT2>* UHMesh::GetUV0Buffer() const
{
	return UV0Buffer.get();
}

UHRenderBuffer<XMFLOAT3>* UHMesh::GetNormalBuffer() const
{
	return NormalBuffer.get();
}

UHRenderBuffer<XMFLOAT4>* UHMesh::GetTangentBuffer() const
{
	return TangentBuffer.get();
}

UHRenderBuffer<uint32_t>* UHMesh::GetIndexBuffer() const
{
	return IndexBuffer.get();
}

UHRenderBuffer<uint16_t>* UHMesh::GetIndexBuffer16() const
{
	return IndexBuffer16.get();
}

UHAccelerationStructure* UHMesh::GetBottomLevelAS() const
{
	return BottomLevelAS.get();
}

int32_t UHMesh::GetHighestIndex() const
{
	return HighestIndex;
}

// function for importing UHAsset
bool UHMesh::Import(std::filesystem::path InUHMeshPath)
{
	std::ifstream FileIn(InUHMeshPath.string().c_str(), std::ios::in | std::ios::binary);
	if (!FileIn.is_open())
	{
		UHE_LOG(L"Failed to Load UHMesh " + InUHMeshPath.wstring() + L"!\n");
		return false;
	}

	// read mesh name
	UHUtilities::ReadStringData(FileIn, Name);

	// read imported material name
	UHUtilities::ReadStringData(FileIn, ImportedMaterialName);

	// read transform data
	FileIn.read(reinterpret_cast<char*>(&ImportedTranslation), sizeof(ImportedTranslation));
	FileIn.read(reinterpret_cast<char*>(&ImportedRotation), sizeof(ImportedRotation));
	FileIn.read(reinterpret_cast<char*>(&ImportedScale), sizeof(ImportedScale));

	// read vertex
	UHUtilities::ReadVectorData(FileIn, PositionData);
	UHUtilities::ReadVectorData(FileIn, UV0Data);
	UHUtilities::ReadVectorData(FileIn, NormalData);
	UHUtilities::ReadVectorData(FileIn, TangentData);

	// read indices
	UHUtilities::ReadVectorData(FileIn, IndicesData);

	FileIn.close();

	VertexCount = static_cast<uint32_t>(PositionData.size());
	IndiceCount = static_cast<uint32_t>(IndicesData.size());

	// calc the mesh center
	constexpr float Inf = std::numeric_limits<float>::infinity();
	XMFLOAT3 MinPoint = XMFLOAT3(Inf, Inf, Inf);
	XMFLOAT3 MaxPoint = XMFLOAT3(-Inf, -Inf, -Inf);

	for (const XMFLOAT3& P : PositionData)
	{
		MinPoint = MathHelpers::MinVector(P, MinPoint);
		MaxPoint = MathHelpers::MaxVector(P, MaxPoint);
	}

	MeshCenter = (MinPoint + MaxPoint) * 0.5f;

	// find the highest indice in the index buffer
	HighestIndex = -1;
	for (const int32_t& Index : IndicesData)
	{
		HighestIndex = (std::max)(Index, HighestIndex);
	}

	// decide the index format with HighestIndex
	if (HighestIndex >= static_cast<int32_t>(std::numeric_limits<uint16_t>::max()))
	{
		bIndexBuffer32Bit = true;
	}

	if (!bIndexBuffer32Bit)
	{
		// prepare 16-bit index data
		IndicesData16.resize(IndiceCount);
		for (uint32_t Idx = 0; Idx < IndiceCount; Idx++)
		{
			IndicesData16[Idx] = static_cast<uint16_t>(IndicesData[Idx]);
		}
	}

	return true;
}

#if WITH_DEBUG
void UHMesh::SetImportedTransform(XMFLOAT3 InTranslation, XMFLOAT3 InRotation, XMFLOAT3 InScale)
{
	ImportedTranslation = InTranslation;
	ImportedRotation = InRotation;
	ImportedScale = InScale;

	// check invalid data
	if (!MathHelpers::IsValidVector(ImportedTranslation))
	{
		ImportedTranslation = XMFLOAT3(0, 0, 0);
	}

	if (!MathHelpers::IsValidVector(ImportedRotation))
	{
		ImportedRotation = XMFLOAT3(0, 0, 0);
	}

	if (!MathHelpers::IsValidVector(ImportedScale) || MathHelpers::IsVectorNearlyZero(ImportedScale))
	{
		ImportedScale = XMFLOAT3(1, 1, 1);
	}
}

void UHMesh::SetImportedMaterialName(std::string InName)
{
	ImportedMaterialName = InName;
}

void UHMesh::ApplyUnitScale()
{
	// apply unit scale to imported mesh
	float UnitScale = 0.01f;
	XMMATRIX S = XMMatrixScaling(UnitScale, UnitScale, UnitScale);

	for (XMFLOAT3& Data : PositionData)
	{
		XMVECTOR P = XMLoadFloat3(&Data);
		P = XMVector3Transform(P, S);
		XMStoreFloat3(&Data, P);
	}

	// imported transform needs to be adjust as well (except scale)
	XMVECTOR T = XMLoadFloat3(&ImportedTranslation);
	XMVECTOR R = XMLoadFloat3(&ImportedRotation);

	T = XMVector3Transform(T, S);
	R = XMVector3Transform(R, S);

	XMStoreFloat3(&ImportedTranslation, T);
	XMStoreFloat3(&ImportedRotation, R);
}

void UHMesh::Export(std::filesystem::path OutputFolder, bool bOverwrite)
{
	// export UHMesh as file, so we don't need to load from source everytime
	// the data output be like:
	// (1) mesh name
	// (2) mesh data

	// create folder if it's not existed
	if (!std::filesystem::exists(OutputFolder))
	{
		std::filesystem::create_directories(OutputFolder);
	}

	// don't output again if file exists if it doesn't want to overwrite
	std::filesystem::path OutPath = OutputFolder.string() + Name + GMeshAssetExtension;
	if (!bOverwrite && std::filesystem::exists(OutPath))
	{
		return;
	}

	// open UHMesh file
	std::ofstream FileOut(OutPath.string(), std::ios::out | std::ios::binary);

	// write mesh name
	UHUtilities::WriteStringData(FileOut, Name);

	// write imported material name
	UHUtilities::WriteStringData(FileOut, ImportedMaterialName);

	// write transform data
	FileOut.write(reinterpret_cast<const char*>(&ImportedTranslation), sizeof(ImportedTranslation));
	FileOut.write(reinterpret_cast<const char*>(&ImportedRotation), sizeof(ImportedRotation));
	FileOut.write(reinterpret_cast<const char*>(&ImportedScale), sizeof(ImportedScale));

	// write vertex
	UHUtilities::WriteVectorData(FileOut, PositionData);
	UHUtilities::WriteVectorData(FileOut, UV0Data);
	UHUtilities::WriteVectorData(FileOut, NormalData);
	UHUtilities::WriteVectorData(FileOut, TangentData);

	// write indices
	UHUtilities::WriteVectorData(FileOut, IndicesData);

	FileOut.close();

	VertexCount = static_cast<uint32_t>(PositionData.size());
	IndiceCount = static_cast<uint32_t>(IndicesData.size());
}
#endif