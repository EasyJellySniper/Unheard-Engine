// shader to soften ray tracing shadows
// based on PCSS algorithm

// store result
RWTexture2DArray<float> OutRTSoftShadow : register(u1);
cbuffer RTSoftShadowConstants : register(b2)
{
    // 1 = 3x3, 2 = 5x5..etc, will be clamped between 1 to 3
    int GPCSSKernal;
    float GPCSSMinPenumbra;
    float GPCSSMaxPenumbra;
    float GPCSSBlockerDistScale;
    
    float4 GSoftShadowResolution;
}

Texture2DArray<float2> InputRTShadowData : register(t3);
Texture2D DepthTexture : register(t4);
Texture2D MipRateTex : register(t5);

#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#define UHSPOTLIGHT_BIND t8
ByteAddressBuffer PointLightList : register(t9);
ByteAddressBuffer SpotLightList : register(t10);

SamplerState PointClampped : register(s11);
SamplerState LinearClampped : register(s12);

#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "UHRTCommon.hlsli"

float Shadow3x3(float3 UV, float BaseShadow, float BaseDepth, float Penumbra, float MipWeight)
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
            float2 Offset = float2(I, J) * Penumbra * GSoftShadowResolution.zw;
            float2 LightUV = UV.xy + Offset;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseShadow;
            }
            else
            {
                Result += InputRTShadowData.SampleLevel(LinearClampped, float3(LightUV, UV.z), 0).g;
            }
        }
    }
    Result *= 0.11111111f;
    
    return Result;
}

float Shadow5x5(float3 UV, float BaseShadow, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -2; I <= 2; I++)
    {
		UHUNROLL
        for (int J = -2; J <= 2; J++)
        {
            float2 Offset = float2(I, J) * Penumbra * GSoftShadowResolution.zw;
            float2 LightUV = UV.xy + Offset;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseShadow;
            }
            else
            {
                Result += InputRTShadowData.SampleLevel(LinearClampped, float3(LightUV, UV.z), 0).g;
            }
        }
    }
    Result *= 0.04f;
    
    return Result;
}

float Shadow7x7(float3 UV, float BaseShadow, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float Result = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -3; I <= 3; I++)
    {
		UHUNROLL
        for (int J = -3; J <= 3; J++)
        {
            float2 Offset = float2(I, J) * Penumbra * GSoftShadowResolution.zw;
            float2 LightUV = UV.xy + Offset;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, LightUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Result += BaseShadow;
            }
            else
            {
                Result += InputRTShadowData.SampleLevel(LinearClampped, float3(LightUV, UV.z), 0).g;
            }
        }
    }
    Result *= 0.02040816f;
    
    return Result;
}

float PCSS(float3 UV, float BaseDepth, float MipWeight)
{
    float CenterShadow = InputRTShadowData.SampleLevel(LinearClampped, UV, 0).g;
    
	UHBRANCH
    if (CenterShadow == 1.0f)
    {
        return CenterShadow;
    }
    
    float DistToBlocker = InputRTShadowData.SampleLevel(LinearClampped, UV, 0).r;
    float Penumbra = lerp(GPCSSMinPenumbra, GPCSSMaxPenumbra, saturate(DistToBlocker * GPCSSBlockerDistScale));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
    Penumbra = lerp(Penumbra, GPCSSMinPenumbra, pow(MipWeight, 0.25f));
 
    float Result = 0;
    
    // select desired quality
    UHBRANCH
    if (GPCSSKernal == 1)
    {
        Result = Shadow3x3(UV, CenterShadow, BaseDepth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 2)
    {
        Result = Shadow5x5(UV, CenterShadow, BaseDepth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 3)
    {
        Result = Shadow7x7(UV, CenterShadow, BaseDepth, Penumbra, MipWeight);
    }
    
    return Result;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void SoftRTShadowCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= GSoftShadowResolution.x || DTid.y >= GSoftShadowResolution.y)
    {
        return;
    }
    
    uint2 PixelCoord = DTid.xy;
    float2 UV = float2(PixelCoord + 0.5f) * GSoftShadowResolution.zw;
    float BaseDepth = DepthTexture.SampleLevel(PointClampped, UV.xy, 0).r;
    
    for (uint Idx = 0; Idx < GMaxSoftShadowLightsPerPixel; Idx++)
    {
        OutRTSoftShadow[uint3(PixelCoord, Idx)] = 1.0f;
    }
    
    UHBRANCH
    if (BaseDepth == 0.0f || !HasLighting())
    {
        return;
    }
    
    // fetch mip weight
    float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;
    float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(UV, BaseDepth);
    
    // PCSS for various lights
    uint SoftShadowCount = 0;
    
    // dir lights
    for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
    {
        if (!UHDirLights[Ldx].bIsEnabled || SoftShadowCount >= GMaxSoftShadowLightsPerPixel)
        {
            continue;
        }
        
        float SoftShadow = PCSS(float3(UV, SoftShadowCount), BaseDepth, MipWeight);
        OutRTSoftShadow[uint3(PixelCoord, SoftShadowCount)] = SoftShadow;
        SoftShadowCount++;
    }
    
    // point lights
    uint2 TileCoordinate = PixelCoord.xy * GResolution.xy * GSoftShadowResolution.zw;
    uint TileX = CoordToTileX(TileCoordinate.x);
    uint TileY = CoordToTileY(TileCoordinate.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    
    uint ClosestPointLightIndex = GetClosestPointLightIndex(PointLightList, TileIndex, WorldPos);
    UHBRANCH
    if (ClosestPointLightIndex != ~0 && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
    {
        float SoftShadow = PCSS(float3(UV, SoftShadowCount), BaseDepth, MipWeight);
        OutRTSoftShadow[uint3(PixelCoord, SoftShadowCount)] = SoftShadow;
        SoftShadowCount++;
    }
    
    // spot lights
    uint ClosestSpotLightIndex = GetClosestSpotLightIndex(SpotLightList, TileIndex, WorldPos);
    UHBRANCH
    if (ClosestSpotLightIndex != ~0 && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
    {
        float SoftShadow = PCSS(float3(UV, SoftShadowCount), BaseDepth, MipWeight);
        OutRTSoftShadow[uint3(PixelCoord, SoftShadowCount)] = SoftShadow;
        SoftShadowCount++;
    }
}