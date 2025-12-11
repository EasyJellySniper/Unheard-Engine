// shader to refine eye vector used in ray tracing reflection
#include "../UHInputs.hlsli"
#include "../UHMaterialCommon.hlsli"
#include "UHRTCommon.hlsli"

RWTexture2D<float4> OutNormal : register(u1);
Texture2D OpaqueNormalTexture : register(t2);
SamplerState LinearSampler : register(s3);

// 2x downsized refine
#define REFINE_DOWNSIZE_FACTOR 2

// 3x3 filter, can consider higher settings if necessary
#define MAX_REFINE_RADIUS 1
groupshared float4 GNormalCache[UHTHREAD_GROUP1D + 2 * MAX_REFINE_RADIUS];

float4 SelectCandidateNormal(int2 PixelCoord)
{
    float2 UV = float2(PixelCoord) * GResolution.zw * REFINE_DOWNSIZE_FACTOR;
    float4 OpaqueN = OpaqueNormalTexture.SampleLevel(LinearSampler, UV, 0);
    
    return float4(DecodeNormal(OpaqueN.xyz), OpaqueN.a > 0 ? 1 : 0);
}

[numthreads(UHTHREAD_GROUP1D, 1, 1)]
void RTSmoothNormalHorizontalCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GResolution.xy / REFINE_DOWNSIZE_FACTOR - 1);
    OutNormal[PixelCoord] = 0;
    
    float2 UV = float2(PixelCoord) * GResolution.zw * REFINE_DOWNSIZE_FACTOR;

    // left edge case
    if (GTid.x < MAX_REFINE_RADIUS)
    {
        int x = max(PixelCoord.x - MAX_REFINE_RADIUS, 0);
        GNormalCache[GTid.x] = SelectCandidateNormal(int2(x, PixelCoord.y));
    }
    
    // right edge case
    if (GTid.x >= UHTHREAD_GROUP1D - MAX_REFINE_RADIUS)
    {
        int x = min(PixelCoord.x + MAX_REFINE_RADIUS, GResolution.x - 1);
        GNormalCache[GTid.x + 2 * MAX_REFINE_RADIUS] = SelectCandidateNormal(int2(x, PixelCoord.y));
    }
    
    float4 CenterN = SelectCandidateNormal(PixelCoord.xy);
    GNormalCache[GTid.x + MAX_REFINE_RADIUS] = CenterN;
    GroupMemoryBarrierWithGroupSync();
    
    float3 AvgNormal = 0;
    float Weight = 0.0f;
    
    UHUNROLL
    for (int I = -MAX_REFINE_RADIUS; I <= MAX_REFINE_RADIUS; I++)
    {
        int KernalIndex = I + MAX_REFINE_RADIUS;
        int CacheIndex = GTid.x + KernalIndex;
        
        float4 CacheN = GNormalCache[CacheIndex];
        AvgNormal += CacheN.xyz;
        Weight += CacheN.w > 0 ? 1 : 0;
    }
    
    AvgNormal /= max(Weight, 1.0f);
    AvgNormal = normalize(AvgNormal);
    OutNormal[PixelCoord] = float4(AvgNormal, CenterN.a);
}

// vertical run is treated as the second run so it does not have to call SelectCandidateReflect()
[numthreads(1, UHTHREAD_GROUP1D, 1)]
void RTSmoothNormalVerticalCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GResolution.xy / REFINE_DOWNSIZE_FACTOR - 1);

    // top edge case
    if (GTid.y < MAX_REFINE_RADIUS)
    {
        int y = max(PixelCoord.y - MAX_REFINE_RADIUS, 0);
        GNormalCache[GTid.y] = OutNormal[int2(PixelCoord.x, y)];
    }
    
    // bottom edge case
    if (GTid.y >= UHTHREAD_GROUP1D - MAX_REFINE_RADIUS)
    {
        int y = min(PixelCoord.y + MAX_REFINE_RADIUS, GResolution.y - 1);
        GNormalCache[GTid.y + 2 * MAX_REFINE_RADIUS] = OutNormal[int2(PixelCoord.x, y)];
    }
    
    float4 CenterN = OutNormal[PixelCoord.xy];
    GNormalCache[GTid.y + MAX_REFINE_RADIUS] = CenterN;
    GroupMemoryBarrierWithGroupSync();
    
    float3 AvgNormal = 0;
    float Weight = 0.0f;
    
    UHUNROLL
    for (int I = -MAX_REFINE_RADIUS; I <= MAX_REFINE_RADIUS; I++)
    {
        int KernalIndex = I + MAX_REFINE_RADIUS;
        int CacheIndex = GTid.y + KernalIndex;
        
        float4 CacheN = GNormalCache[CacheIndex];
        AvgNormal += CacheN.xyz;
        Weight += CacheN.w > 0 ? 1 : 0;
    }
    
    AvgNormal /= max(Weight, 1.0f);
    AvgNormal = normalize(AvgNormal);
    OutNormal[PixelCoord] = float4(AvgNormal, CenterN.a);
}