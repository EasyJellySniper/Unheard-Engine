#include "UHInputs.hlsli"

RWTexture2D<float4> Face0 : register(u0);
RWTexture2D<float4> Face1 : register(u1);
RWTexture2D<float4> Face2 : register(u2);
RWTexture2D<float4> Face3 : register(u3);
RWTexture2D<float4> Face4 : register(u4);
RWTexture2D<float4> Face5 : register(u5);

cbuffer SmoothCubemapConstant : register(b6)
{
    uint GTextureSize;
}

void AverageFaceCorners()
{
    uint Edge = GTextureSize - 1;
    float4 Result;
    
    // average all corners of faces based on cubemap layout
    //   2
    // 1 4 0 5
    //   3
    
    // avg face4 - corner points only
    {
        Result = (Face4[uint2(0, 0)] + Face2[uint2(0, Edge)] + Face1[uint2(Edge, 0)]) * 0.33333333f;
        Face4[uint2(0, 0)] = Result;
        Face2[uint2(0, Edge)] = Result;
        Face1[uint2(Edge, 0)] = Result;

        Result = (Face4[uint2(0, Edge)] + Face3[uint2(0, 0)] + Face1[uint2(Edge, Edge)]) * 0.33333333f;
        Face4[uint2(0, Edge)] = Result;
        Face3[uint2(0, 0)] = Result;
        Face1[uint2(Edge, Edge)] = Result;
        
        Result = (Face4[uint2(Edge, 0)] + Face2[uint2(Edge, Edge)] + Face0[uint2(0, 0)]) * 0.33333333f;
        Face4[uint2(Edge, 0)] = Result;
        Face2[uint2(Edge, Edge)] = Result;
        Face0[uint2(0, 0)] = Result;
        
        Result = (Face4[uint2(Edge, Edge)] + Face3[uint2(Edge, 0)] + Face0[uint2(0, Edge)]) * 0.33333333f;
        Face4[uint2(Edge, Edge)] = Result;
        Face3[uint2(Edge, 0)] = Result;
        Face0[uint2(0, Edge)] = Result;
    }
    
    // avg face5 - corner points only
    {
        Result = (Face5[uint2(0, 0)] + Face2[uint2(Edge, 0)] + Face0[uint2(Edge, 0)]) * 0.33333333f;
        Face5[uint2(0, 0)] = Result;
        Face2[uint2(Edge, 0)] = Result;
        Face0[uint2(Edge, 0)] = Result;
        
        Result = (Face5[uint2(0, Edge)] + Face3[uint2(Edge, Edge)] + Face0[uint2(Edge, Edge)]) * 0.33333333f;
        Face5[uint2(0, Edge)] = Result;
        Face3[uint2(Edge, Edge)] = Result;
        Face0[uint2(Edge, Edge)] = Result;
        
        Result = (Face5[uint2(Edge, 0)] + Face1[uint2(0, 0)] + Face2[uint2(0, 0)]) * 0.33333333f;
        Face5[uint2(Edge, 0)] = Result;
        Face1[uint2(0, 0)] = Result;
        Face2[uint2(0, 0)] = Result;
        
        Result = (Face5[uint2(Edge, Edge)] + Face1[uint2(0, Edge)] + Face3[uint2(0, Edge)]) * 0.33333333f;
        Face5[uint2(Edge, Edge)] = Result;
        Face1[uint2(0, Edge)] = Result;
        Face3[uint2(0, Edge)] = Result;
    }
}

void AverageFaceEdges(uint Pos)
{
    uint Edge = GTextureSize - 1;
    
    // each face has 4 edges to average, make sure code below won't do duplicate average
    uint2 LeftEdge = uint2(0, Pos);
    uint2 RightEdge = uint2(Edge, Pos);
    
    uint2 InvLeftEdge = uint2(0, GTextureSize - Pos - 1);
    uint2 InvRightEdge = uint2(Edge, GTextureSize - Pos - 1);
    
    uint2 TopEdge = uint2(Pos, 0);
    uint2 InvTopEdge = uint2(GTextureSize - Pos - 1, 0);
    uint2 BottomEdge = uint2(Pos, Edge);
    uint2 InvBottomEdge = uint2(GTextureSize - Pos - 0, Edge);
    
    float4 Result;
    
    // average all edges of faces based on cubemap layout
    //   2
    // 1 4 0 5
    //   3
    
    // avg face0
    {
        // left edge
        Result = (Face0[LeftEdge] + Face4[RightEdge]) * 0.5f;
        Face0[LeftEdge] = Result;
        Face4[RightEdge] = Result;
        
        // right edge
        Result = (Face5[LeftEdge] + Face0[RightEdge]) * 0.5f;
        Face5[LeftEdge] = Result;
        Face0[RightEdge] = Result;
        
        // top edge
        Result = (Face2[InvRightEdge] + Face0[TopEdge]) * 0.5f;
        Face2[InvRightEdge] = Result;
        Face0[TopEdge] = Result;
        
        // bottom edge
        Result = (Face3[RightEdge] + Face0[BottomEdge]) * 0.5f;
        Face3[RightEdge] = Result;
        Face0[BottomEdge] = Result;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // avg face1
    {
        // left edge
        Result = (Face1[LeftEdge] + Face5[RightEdge]) * 0.5f;
        Face1[LeftEdge] = Result;
        Face5[RightEdge] = Result;
        
        // right edge
        Result = (Face4[LeftEdge] + Face1[RightEdge]) * 0.5f;
        Face4[LeftEdge] = Result;
        Face1[RightEdge] = Result;
        
        // top edge
        Result = (Face2[LeftEdge] + Face1[TopEdge]) * 0.5f;
        Face2[LeftEdge] = Result;
        Face1[TopEdge] = Result;
        
        // bottom edge
        Result = (Face3[LeftEdge] + Face1[BottomEdge]) * 0.5f;
        Face3[LeftEdge] = Result;
        Face1[BottomEdge] = Result;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // avg face2 - top down only
    {
        // bottom edge
        Result = (Face2[BottomEdge] + Face4[TopEdge]) * 0.5f;
        Face2[BottomEdge] = Result;
        Face4[TopEdge] = Result;
        
        // top edge
        Result = (Face2[TopEdge] + Face5[InvTopEdge]) * 0.5f;
        Face2[TopEdge] = Result;
        Face5[InvTopEdge] = Result;
        GroupMemoryBarrierWithGroupSync();
    }
    
    // avg face3 - top down only
    {
        // bottom edge
        Result = (Face3[BottomEdge] + Face5[InvBottomEdge]) * 0.5f;
        Face3[BottomEdge] = Result;
        Face5[InvBottomEdge] = Result;
        
        // top edge
        Result = (Face3[TopEdge] + Face4[BottomEdge]) * 0.5f;
        Face3[TopEdge] = Result;
        Face4[BottomEdge] = Result;
    }
}

[numthreads(UHTHREAD_GROUP2D_X, 1, 1)]
void SmoothCubemapCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= GTextureSize)
    {
        return;
    }
    
    // average face corners with the first thread only
    if (DTid.x == 0)
    {
        AverageFaceCorners();
    }
    GroupMemoryBarrierWithGroupSync();
    
    // then average the face edges
    if (DTid.x > 0 && DTid.x < GTextureSize - 1)
    {
        AverageFaceEdges(DTid.x);
    }
}