#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#define SH9_BIND t6
#include "UHSphericalHamonricCommon.hlsli"

// output indirect light result
RWTexture2D<float4> IndirectLightResult : register(u1);
Texture2DArray<float4> RTIndirectLight : register(t3);
Texture2D SceneMixedDepth : register(t4);
Texture2D MotionTexture : register(t5);
SamplerState LinearClampped : register(s7);

static const uint GDownsampleFactor = 2;

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void IndirectLightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x * GDownsampleFactor >= GResolution.x || DTid.y * GDownsampleFactor >= GResolution.y)
    {
        return;
    }
    
    uint2 OutputCoord = DTid.xy;
    uint2 PixelCoord = OutputCoord * GDownsampleFactor;
    IndirectLightResult[OutputCoord] = float4(0, 0, 0, 1);
    
    float Depth = SceneMixedDepth[PixelCoord].r;
    UHBRANCH
    if (Depth == 0.0f)
    {
        return;
    }
    
    // SH9 for fallback
    float4 Diffuse = SceneBuffers[0][PixelCoord];
    float3 Normal = DecodeNormal(SceneBuffers[1][PixelCoord].xyz);
    float3 SkyLight = ShadeSH9(Diffuse.rgb, float4(Normal, 1.0f), Diffuse.a);
    
    float3 Result = 0;
    float Occlusion = 0;
    
    UHBRANCH
    if (GSystemRenderFeature & UH_RT_INDIRECTLIGHT)
    {
        float2 ScreenUV = float2(PixelCoord + 0.5f) * GResolution.zw;
        float2 Motion = MotionTexture.SampleLevel(LinearClampped, ScreenUV, 0).rg;
        float2 HistoryUV = ScreenUV - Motion;
        uint CurrentFrameIndex = GFrameNumber & 1;
    
        // sample indirect lighting
        for (uint Idx = 0; Idx < GNumOfIndirectFrames; Idx++)
        {
            float2 IndirectUV = lerp(HistoryUV, ScreenUV, CurrentFrameIndex == Idx);
            float4 IL = RTIndirectLight.SampleLevel(LinearClampped, float3(IndirectUV, Idx), 0);
            Result += SkyLight * IL.a + IL.rgb;
            Occlusion += IL.a;
        }
    
        Result *= 0.5f;
        Occlusion *= 0.5f;
    }
    else
    {
        Result = SkyLight;
        Occlusion = 1.0f;
    }
    
    IndirectLightResult[OutputCoord] = float4(Result, Occlusion);
}