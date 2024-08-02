// base amplification shader in UHE
#include "../Shaders/UHMeshShaderCommon.hlsli"

// visiable renderer index is stored in this buffer.
// access via DTid first, then use this index for renderer instances
ByteAddressBuffer VisibleRendererIndex : register(t3);

// parameters
struct ASParameter
{
    uint MaxInstance;
};
StructuredBuffer<ASParameter> ASData : register(t4);
StructuredBuffer<UHRendererInstance> RendererInstances : register(t5);

groupshared UHRendererInstance GInstance;

[NumThreads(MESHSHADER_GROUP_SIZE, 1, 1)]
void MainAS(uint GTid : SV_GroupThreadID, uint DTid : SV_DispatchThreadID, uint Gid : SV_GroupID)
{
    if (DTid >= ASData[0].MaxInstance)
    {
        return;
    }
    
    // dispatch meshlets and pass the instance as payload
    uint RendererIdx = VisibleRendererIndex.Load(DTid * 4);
    UHRendererInstance Instance = RendererInstances[RendererIdx];
    GInstance = Instance;
    DispatchMesh(Instance.NumMeshlets, 1, 1, GInstance);
}