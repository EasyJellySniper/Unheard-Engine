// base amplification shader in UHE
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMeshShaderCommon.hlsli"

// for occlusion test in AS
StructuredBuffer<UHMeshShaderData> MeshShaderData : register(t3);
ByteAddressBuffer OcclusionResult : register(t4);

groupshared uint GVisibleCount;
groupshared UHMeshPayload Payload;

// C++ side: Dispatch as (TotalMeshlets / MESHSHADER_GROUP_SIZE)
[NumThreads(MESHSHADER_GROUP_SIZE, 1, 1)]
void BaseAS(uint DTid : SV_DispatchThreadID, uint GTid : SV_GroupThreadID)
{    
    if (GTid == 0)
    {
        GVisibleCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    // @TODO: Check if DTid exceeds MeshShaderData count.
    
    // count unoccluded meshlets to dispatch
    UHMeshShaderData ShaderData = MeshShaderData[DTid];
    if (ShaderData.bDoOcclusionTest == 0 || OcclusionResult.Load(ShaderData.RendererIndex * 4) > 0)
    {
        uint StoreIdx = 0;
        InterlockedAdd(GVisibleCount, 1, StoreIdx);
        Payload.ShaderDataIndices[StoreIdx] = DTid;
    }
    
    DispatchMesh(GVisibleCount, 1, 1, Payload);
}