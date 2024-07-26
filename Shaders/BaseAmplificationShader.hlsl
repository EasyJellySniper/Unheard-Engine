// base amplification shader in UHE
#include "../Shaders/UHMeshShaderCommon.hlsli"

// visible instances will be collected & sorted on CPU side, then be dispatched
// in the AS shader, an instance will further dispatch necessary meshlets and store info in payload
StructuredBuffer<UHRendererInstance> RendererInstances : register(t3);

// parameters
struct ASParameter
{
    uint MaxInstance;
};
StructuredBuffer<ASParameter> ASData : register(t4);

groupshared UHRendererInstance GInstance;

[NumThreads(MESHSHADER_GROUP_SIZE, 1, 1)]
void MainAS(uint GTid : SV_GroupThreadID, uint DTid : SV_DispatchThreadID, uint Gid : SV_GroupID)
{
    if (DTid >= ASData[0].MaxInstance)
    {
        return;
    }
    
    // dispatch meshlets and pass the instance as payload
    UHRendererInstance Instance = RendererInstances[DTid];
    GInstance = Instance;
    DispatchMesh(Instance.NumMeshlets, 1, 1, GInstance);
}