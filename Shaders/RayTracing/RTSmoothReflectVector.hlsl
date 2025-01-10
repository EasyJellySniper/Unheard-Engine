// shader to refine eye vector used in ray tracing reflection
#include "../UHInputs.hlsli"
#include "../UHMaterialCommon.hlsli"
#include "UHRTCommon.hlsli"

RWTexture2D<float4> OutNormal : register(u1);
Texture2D OpaqueNormalTexture : register(t2);
Texture2D TranslucentNormalTexture : register(t3);
Texture2D MixedDepthTexture : register(t4);
SamplerState LinearSampler : register(s5);

// 2x downsized refine
#define REFINE_DOWNSIZE_FACTOR 2

// 5x5 filter
#define MAX_REFINE_RADIUS 2
groupshared float4 GNormalCache[UHTHREAD_GROUP1D + 2 * MAX_REFINE_RADIUS];

float4 SelectCandidateReflect(float3 EyeVector, int2 PixelCoord)
{
    float2 UV = float2(PixelCoord) * GResolution.zw * REFINE_DOWNSIZE_FACTOR;
    float4 OpaqueN = OpaqueNormalTexture.SampleLevel(LinearSampler, UV, 0);
    float4 TranslucentN = TranslucentNormalTexture.SampleLevel(LinearSampler, UV, 0);
    float4 CandidateN = TranslucentN.a > 0 ? TranslucentN : OpaqueN;
    
    return float4(reflect(EyeVector, DecodeNormal(CandidateN.xyz)), CandidateN.a > 0 ? 1 : 0);
}

[numthreads(UHTHREAD_GROUP1D, 1, 1)]
void RTSmoothReflectHorizontalCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GResolution.xy / REFINE_DOWNSIZE_FACTOR - 1);
    OutNormal[PixelCoord] = 0;
    
    float2 UV = float2(PixelCoord) * GResolution.zw * REFINE_DOWNSIZE_FACTOR;
    float SceneDepth = MixedDepthTexture.SampleLevel(LinearSampler, UV, 0).r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(UV, SceneDepth);
    float3 EyeVector = normalize(SceneWorldPos - GCameraPos);

    // left edge case
    if (GTid.x < MAX_REFINE_RADIUS)
    {
        int x = max(PixelCoord.x - MAX_REFINE_RADIUS, 0);
        GNormalCache[GTid.x] = SelectCandidateReflect(EyeVector, int2(x, PixelCoord.y));
    }
    
    // right edge case
    if (GTid.x >= UHTHREAD_GROUP1D - MAX_REFINE_RADIUS)
    {
        int x = min(PixelCoord.x + MAX_REFINE_RADIUS, GResolution.x - 1);
        GNormalCache[GTid.x + 2 * MAX_REFINE_RADIUS] = SelectCandidateReflect(EyeVector, int2(x, PixelCoord.y));
    }
    
    float4 CenterN = SelectCandidateReflect(EyeVector, PixelCoord.xy);
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
void RTSmoothReflectVerticalCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
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