#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

#define SH9_BIND t5
#include "UHSphericalHamonricCommon.hlsli"

// output indirect light result
RWTexture2D<float4> IndirectLightResult : register(u1);

Texture2DArray<float4> RTIndirectLight : register(t3);
Texture2D SceneMixedDepth : register(t4);
SamplerState LinearClampped : register(s6);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void IndirectLightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
    {
        return;
    }
    
    uint2 PixelCoord = DTid.xy;
    IndirectLightResult[PixelCoord] = 0;
    
    float Depth = SceneMixedDepth[PixelCoord].r;
    UHBRANCH
    if (Depth == 0.0f)
    {
        return;
    }
    
    // reconstruct world position
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(DTid.xy + 0.5f), Depth, true);
    float2 IndirectLightUV = float2(PixelCoord + 0.5f) * GResolution.zw;
    
    // SH9 for fallback
    float4 Diffuse = SceneBuffers[0][PixelCoord];
    float3 Normal = DecodeNormal(SceneBuffers[1][PixelCoord].xyz);
    float3 SkyLight = ShadeSH9(Diffuse.rgb, float4(Normal, 1.0f), Diffuse.a);
    
    // sample indirect lighting
    float3 Result = 0;
    float Occlusion = 0;
    for (uint Idx = 0; Idx < 4; Idx++)
    {
        float4 IL = RTIndirectLight.SampleLevel(LinearClampped, float3(IndirectLightUV, Idx), 0);
        Result += SkyLight * IL.a + IL.rgb;
        Occlusion += IL.a;
    }
    
    Result *= 0.25f;
    Occlusion *= 0.25f;
    
    IndirectLightResult[PixelCoord] = float4(Result, Occlusion);
}