// base mesh shader in UHE
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMeshShaderCommon.hlsli"

// object constants
StructuredBuffer<ObjectConstants> RendererConstants : register(t1);

// all mesh data, fetched by mesh index first
StructuredBuffer<UHMeshlet> Meshlets[] : register(t0, space3);
StructuredBuffer<float3> PositionBuffer[] : register(t0, space4);
StructuredBuffer<float2> UV0Buffer[] : register(t0, space5);
ByteAddressBuffer IndicesBuffer[] : register(t0, space6);

// entry point for mesh shader
// each group should process all verts and prims of a meshlet, up to MESHSHADER_MAX_VERTEX & MESHSHADER_MAX_PRIMITIVE
[NumThreads(MESHSHADER_GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void DepthMS(
    uint DTid : SV_DispatchThreadID,
    uint GTid : SV_GroupThreadID,
    uint Gid : SV_GroupID,
    in payload UHRendererInstance InInstance,
    out vertices DepthVertexOutput OutVerts[MESHSHADER_MAX_VERTEX],
    out indices uint3 OutTris[MESHSHADER_MAX_PRIMITIVE]
)
{
    uint RendererIndex = InInstance.RendererIndex;
    uint MeshIndex = InInstance.MeshIndex;
    uint IndiceType = InInstance.IndiceType;
    uint MeshletIndex = Gid;

    UHMeshlet Meshlet = Meshlets[MeshIndex][MeshletIndex];
    SetMeshOutputCounts(Meshlet.VertexCount, Meshlet.PrimitiveCount);
    
    if (GTid < Meshlet.VertexCount)
    {
        // fetch vertex data and output
        DepthVertexOutput Output = (DepthVertexOutput)0;
        Output.Position.xyz = PositionBuffer[MeshIndex][GTid + Meshlet.VertexOffset];
        Output.UV0 = UV0Buffer[MeshIndex][GTid + Meshlet.VertexOffset];
        
        // transformation
        ObjectConstants Constant = RendererConstants[RendererIndex];
        float3 WorldPos = mul(float4(Output.Position.xyz, 1.0f), Constant.GWorld).xyz;

        float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - GCameraPos));
        Output.Position = mul(float4(WorldPos, 1.0f), GViewProj_NonJittered);
        Output.Position = mul(Output.Position, JitterMatrix);
        
        OutVerts[GTid] = Output;
    }

    if (GTid < Meshlet.PrimitiveCount)
    {
        // fetch primitive data and output
        OutTris[GTid] = GetIndices(IndicesBuffer[MeshIndex], GTid + Meshlet.PrimitiveOffset, IndiceType);
    }
}