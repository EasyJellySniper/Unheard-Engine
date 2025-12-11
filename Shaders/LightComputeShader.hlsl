#define UHGBUFFER_BIND t4
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#include "UHLightCommon.hlsli"

cbuffer SoftRTShadowConstants : register(b1)
{
    // 1 = 3x3, 2 = 5x5..etc, will be clamped between 1 to 3
    int GPCSSKernal;
    float GPCSSMinPenumbra;
    float GPCSSMaxPenumbra;
    float GPCSSBlockerDistScale;
}

RWTexture2D<float4> SceneResult : register(u2);
RWTexture2D<float4> IndirectOcclusionResult : register(u3);

Texture2D DirectLightResult : register(t5);
Texture2D DirectHitTexture : register(t6);
Texture2DArray<float4> RTIndirectLight : register(t7);
Texture2D DepthTexture : register(t8);
Texture2D MipTexture : register(t9);
Texture2D MotionTexture : register(t10);

#define SH9_BIND t11
#include "UHSphericalHamonricCommon.hlsli"
SamplerState PointClampped : register(s12);
SamplerState LinearClampped : register(s13);

float3 Light3x3(float2 UV, float3 BaseLight, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float3 Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -1; I <= 1; I++)
    {
		UHUNROLL
        for (int J = -1; J <= 1; J++)
        {
            float2 LightUV = UV + float2(I, J) * Penumbra * GResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseLight;
            }
            else
            {
                Result += DirectLightResult.SampleLevel(LinearClampped, LightUV, 0).rgb;
            }
        }
    }
    Result *= 0.11111111f;
    
    return Result;
}

float3 Light5x5(float2 UV, float3 BaseLight, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float3 Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -2; I <= 2; I++)
    {
		UHUNROLL
        for (int J = -2; J <= 2; J++)
        {
            float2 LightUV = UV + float2(I, J) * Penumbra * GResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseLight;
            }
            else
            {
                Result += DirectLightResult.SampleLevel(LinearClampped, LightUV, 0).rgb;
            }
        }
    }
    Result *= 0.04f;
    
    return Result;
}

float3 Light7x7(float2 UV, float3 BaseLight, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float3 Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -3; I <= 3; I++)
    {
		UHUNROLL
        for (int J = -3; J <= 3; J++)
        {
            float2 LightUV = UV + float2(I, J) * Penumbra * GResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseLight;
            }
            else
            {
                Result += DirectLightResult.SampleLevel(LinearClampped, LightUV, 0).rgb;
            }
        }
    }
    Result *= 0.02040816f;
    
    return Result;
}

float Occlusion3x3(float2 UV, float BaseOcclusion, float BaseDepth, float Penumbra, float MipWeight
    , float SliceIdx)
{
	// actually sampling
    float Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -1; I <= 1; I++)
    {
		UHUNROLL
        for (int J = -1; J <= 1; J++)
        {
            float2 LightUV = UV + float2(I, J) * Penumbra * GResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseOcclusion;
            }
            else
            {
                Result += RTIndirectLight.SampleLevel(LinearClampped, float3(LightUV, SliceIdx), 0).a;
            }
        }
    }
    Result *= 0.11111111f;
    
    return Result;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
	{
		return;
	}

    uint2 PixelCoord = DTid.xy;
    float2 UV = (DTid.xy + 0.5f) * GResolution.zw;
    float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV, 0).r;
    float4 CurrSceneData = SceneResult[PixelCoord];
    IndirectOcclusionResult[PixelCoord] = 1;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
    if (Depth == 0.0f || !HasLighting())
	{
		return;
	}
    
    // ------------------------------------------------------------------------------------------ direct light sampling
    float DistToBlocker = DirectHitTexture.SampleLevel(LinearClampped, UV, 0).r;
    float Penumbra = lerp(GPCSSMinPenumbra, GPCSSMaxPenumbra, saturate(DistToBlocker * GPCSSBlockerDistScale));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
    float MipRate = MipTexture.SampleLevel(LinearClampped, UV, 0).r;
    float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
    Penumbra = lerp(Penumbra, GPCSSMinPenumbra, pow(MipWeight, 0.25f));
    
    float3 DirectResult = DirectLightResult.SampleLevel(LinearClampped, UV, 0).rgb;
    bool bDoSoftShadow = length(DirectResult) == 0.0f;
    
    // select desired quality
    UHBRANCH
    if (GPCSSKernal == 1 && bDoSoftShadow)
    {
        DirectResult = Light3x3(UV, DirectResult, Depth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 2 && bDoSoftShadow)
    {
        DirectResult = Light5x5(UV, DirectResult, Depth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 3 && bDoSoftShadow)
    {
        DirectResult = Light7x7(UV, DirectResult, Depth, Penumbra, MipWeight);
    }
    
    // ------------------------------------------------------------------------------------------ indirect light sampling
    float3 ILResult = 0;
    
    // SH9
    float4 Diffuse = SceneBuffers[0].SampleLevel(LinearClampped, UV, 0);
    float3 Normal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, UV, 0).xyz);
    float Occlusion = 0;
    float3 SkyLight = ShadeSH9(Diffuse.rgb, float4(Normal, 1.0f), 1.0f);
    
    UHBRANCH
    if (GSystemRenderFeature & UH_RT_INDIRECTLIGHT)
    {
        float2 Motion = MotionTexture.SampleLevel(LinearClampped, UV, 0).rg;
        float2 HistoryUV = UV - Motion;
        uint CurrentFrameIndex = GFrameNumber & 1;
        
        // sample indirect lighting
        for (uint Idx = 0; Idx < GNumOfIndirectFrames; Idx++)
        {
            float2 IndirectUV = lerp(HistoryUV, UV, CurrentFrameIndex == Idx);
            float4 IL = RTIndirectLight.SampleLevel(LinearClampped, float3(IndirectUV, Idx), 0);
  
            // 3x3 soft occlusion
            float SoftOcclusion = Occlusion3x3(IndirectUV, IL.a, Depth, Penumbra, MipWeight, Idx);
            float ThisOcclusion = min(SoftOcclusion, Diffuse.a);
            
            ILResult += SkyLight * ThisOcclusion + IL.rgb * Diffuse.rgb;
            Occlusion += ThisOcclusion;
        }
    
        ILResult *= 0.5f;
        Occlusion *= 0.5f;
    }
    else
    {
        Occlusion = Diffuse.a;
        ILResult += SkyLight * Occlusion;
    }
    
    // accumulate results to scene
    SceneResult[PixelCoord] = float4(CurrSceneData.rgb + DirectResult + ILResult, CurrSceneData.a);
    
    // output IL result for reflection pass use
    IndirectOcclusionResult[PixelCoord] = Occlusion;
}