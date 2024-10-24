#ifndef UHMESHSHADERCOMMAND_H
#define UHMESHSHADERCOMMAND_H

#define MESHSHADER_GROUP_SIZE 126
// 126: closest multplt of three to 128
// 42: vertex count / 3
#define MESHSHADER_MAX_VERTEX 126
#define MESHSHADER_MAX_PRIMITIVE 42

// on the C++ side, it will count total number of meshlets to dispatch for each material group
// and this is the list to store the shader to meshlet and renderer data
struct UHMeshShaderData
{
    uint RendererIndex;
    uint MeshletIndex;
    uint bDoOcclusionTest;
};

// meshlet data
struct UHMeshlet
{
    uint VertexCount;
    uint VertexOffset;
    uint PrimitiveCount;
};

struct ObjectConstants
{
    float4x4 GWorld;
    float4x4 GWorldIT;
    float4x4 GPrevWorld;
    uint GInstanceIndex;
    
    // align to 256 bytes, the buffer is used as storage in mesh shader, the structure must be the same as c++ define
    float CPUPadding[15];
};

uint3 GetIndices(ByteAddressBuffer InBuffer, uint InPrimIndex, uint InIndiceType)
{
    // get index data based on indice type, it can be 16 or 32 bit
    uint3 Indices = 0;
    
    if (InIndiceType == 1)
    {
        uint IbStride = 4;
        Indices[0] = InBuffer.Load(InPrimIndex * 3 * IbStride);
        Indices[1] = InBuffer.Load(InPrimIndex * 3 * IbStride + IbStride);
        Indices[2] = InBuffer.Load(InPrimIndex * 3 * IbStride + IbStride * 2);
    }
    else
    {
        uint IbStride = 2;
        uint Offset = InPrimIndex * 3 * IbStride;

        const uint DwordAlignedOffset = Offset & ~3;
        const uint2 Four16BitIndices = InBuffer.Load2(DwordAlignedOffset);

		// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
        if (DwordAlignedOffset == Offset)
        {
            Indices[0] = Four16BitIndices.x & 0xffff;
            Indices[1] = (Four16BitIndices.x >> 16) & 0xffff;
            Indices[2] = Four16BitIndices.y & 0xffff;
        }
        else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
        {
            Indices[0] = (Four16BitIndices.x >> 16) & 0xffff;
            Indices[1] = Four16BitIndices.y & 0xffff;
            Indices[2] = (Four16BitIndices.y >> 16) & 0xffff;
        }
    }
	
    return Indices;
}

uint GetVertexIndex(ByteAddressBuffer InBuffer, uint InIndex, uint InIndiceType)
{
    uint IbStride = 4;
    uint Index = 0;
    
    if (InIndiceType == 1)
    {
        Index = InBuffer.Load(InIndex * IbStride);
    }
    else
    {
        // similar calculation as GetIndices(), but only return one index
        uint WorldOffset = (InIndex & 1);
        uint ByteOffset = (InIndex / 2) * IbStride;

        // Grab a pair of 16-bit indices, and mask out proper 16-bits.
        const uint Two16BitIndices = InBuffer.Load(ByteOffset);
        Index = (Two16BitIndices >> (WorldOffset * 16)) & 0xffff;
    }
    
    return Index;
}

float3 LocalToWorldNormalMS(float3 Normal, float3x3 WorldIT)
{
	// mul(IT_M, norm) => mul(norm, I_M) => {dot(norm, I_M.col0), dot(norm, I_M.col1), dot(norm, I_M.col2)}
    return normalize(mul(Normal, (float3x3)WorldIT));
}

float3 LocalToWorldDirMS(float3 Dir, float3x3 World)
{
    return normalize(mul(Dir, (float3x3)World));
}

float3x3 CreateTBNMS(float3 InWorldNormal, float4 InTangent, float3x3 World)
{
    float3 Tangent = LocalToWorldDirMS(InTangent.xyz, World);
    float3 Binormal = cross(InWorldNormal, Tangent) * InTangent.w;
    Tangent = cross(Binormal, InWorldNormal) * InTangent.w;

    float3x3 TBN = float3x3(Tangent, Binormal, InWorldNormal);
    return TBN;
}

#endif