#ifndef UHMESHSHADERCOMMAND_H
#define UHMESHSHADERCOMMAND_H

#define MESHSHADER_GROUP_SIZE 128
#define MESHSHADER_MAX_VERTEX 128
#define MESHSHADER_MAX_PRIMITIVE 128

struct UHRendererInstance
{
    // renderer index to lookup object constants
    uint RendererIndex;
    // mesh index to lookup mesh data
    uint MeshIndex;
    // num meshlets to dispatch
    uint NumMeshlets;
    // indice type
    uint IndiceType;
};

// meshlet data
struct UHMeshlet
{
    uint VertexCount;
    uint VertexOffset;
    uint PrimitiveCount;
    uint PrimitiveOffset;
};

struct ObjectConstants
{
    float4x4 GWorld;
    float4x4 GWorldIT;
    float4x4 GPrevWorld;
    uint GInstanceIndex;
    uint GNeedWorldTBN;
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

#endif