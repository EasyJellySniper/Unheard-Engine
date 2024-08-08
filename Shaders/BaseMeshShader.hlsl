// base mesh shader in UHE
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMeshShaderCommon.hlsli"

// object constants
StructuredBuffer<ObjectConstants> RendererConstants : register(t1);

// mesh shader data to lookup renderer index & meshlet index
// this should be accessed via Gid, since C++ side is dispatching (TotalMeshlets,1,1)
StructuredBuffer<UHMeshShaderData> MeshShaderData : register(t3);

// renderer instances
StructuredBuffer<UHRendererInstance> RendererInstances : register(t4);

// all mesh data, fetched by mesh index first
StructuredBuffer<UHMeshlet> Meshlets[] : register(t0, space3);
StructuredBuffer<float3> PositionBuffer[] : register(t0, space4);
StructuredBuffer<float2> UV0Buffer[] : register(t0, space5);
ByteAddressBuffer IndicesBuffer[] : register(t0, space6);
StructuredBuffer<float3> NormalBuffer[] : register(t0, space7);
StructuredBuffer<float4> TangentBuffer[] : register(t0, space8);

// entry point for mesh shader
// each group should process all verts and prims of a meshlet, up to MESHSHADER_MAX_VERTEX & MESHSHADER_MAX_PRIMITIVE
[NumThreads(MESHSHADER_GROUP_SIZE, 1, 1)]
[OutputTopology("triangle")]
void BaseMS(
    uint Gid : SV_GroupID,
    uint GTid : SV_GroupThreadID,
    out vertices VertexOutput OutVerts[MESHSHADER_MAX_VERTEX],
    out indices uint3 OutTris[MESHSHADER_MAX_PRIMITIVE]
)
{
    // fetch data and set mesh outputs
    UHMeshShaderData ShaderData = MeshShaderData[Gid];
    UHRendererInstance InInstance = RendererInstances[ShaderData.RendererIndex];
    UHMeshlet Meshlet = Meshlets[InInstance.MeshIndex][ShaderData.MeshletIndex];
    SetMeshOutputCounts(Meshlet.VertexCount, Meshlet.PrimitiveCount);
    
    // output triangles first
    if (GTid < Meshlet.PrimitiveCount)
    {
        // output triangle indices in order, the vertex output below will get the correct unique vertex to output
        // so I don't need to mess around here
        OutTris[GTid] = uint3(GTid * 3 + 0, GTid * 3 + 1, GTid * 3 + 2);
    }
    
    // output vertrex next
    if (GTid < Meshlet.VertexCount)
    {
        // convert local index to vertex index, and lookup the corresponding vertex
        // it's like outputting the "IndicesData" in UHMesh class, which holds the unique vertex indices
        uint VertexIndex = GetVertexIndex(IndicesBuffer[InInstance.MeshIndex], GTid + Meshlet.VertexOffset, InInstance.IndiceType);
        
        // fetch vertex data and output
        VertexOutput Output = (VertexOutput)0;
        Output.Position.xyz = PositionBuffer[InInstance.MeshIndex][VertexIndex];
        Output.UV0 = UV0Buffer[InInstance.MeshIndex][VertexIndex];
        
        // transformation
        ObjectConstants Constant = RendererConstants[ShaderData.RendererIndex];
        float3 WorldPos = mul(float4(Output.Position.xyz, 1.0f), Constant.GWorld).xyz;

        float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - GCameraPos));
        Output.Position = mul(float4(WorldPos, 1.0f), GViewProj_NonJittered);
        Output.Position = mul(Output.Position, JitterMatrix);
        
        float3 Normal = LocalToWorldNormalMS(NormalBuffer[InInstance.MeshIndex][VertexIndex], (float3x3)Constant.GWorldIT);
#if TANGENT_SPACE
	    // calculate world TBN if normal map is used
        Output.WorldTBN = CreateTBNMS(Normal, TangentBuffer[InInstance.MeshIndex][VertexIndex], (float3x3)Constant.GWorld);
#endif
        
        // transform normal by world IT
        Output.Normal = Normal;
        
        OutVerts[GTid] = Output;
    }
}