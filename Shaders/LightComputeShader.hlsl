RWTexture2D<float4> SceneResult : register(u1);
RWTexture2D<float4> IndirectOcclusionResult : register(u2);

#define UHGBUFFER_BIND t3
#define UHDIRLIGHT_BIND t4
#define UHPOINTLIGHT_BIND t5
#define UHSPOTLIGHT_BIND t6
ByteAddressBuffer PointLightList : register(t7);
ByteAddressBuffer SpotLightList : register(t8);
#define SH9_BIND t9

Texture2DArray<float> RTShadowResult : register(t10);
Texture2D<uint> RTReceiveLightBits : register(t11);
Texture2DArray<float4> RTIndirectLight : register(t12);
Texture2D DepthTexture : register(t13);
Texture2D MipTexture : register(t14);
Texture2D MotionTexture : register(t15);

SamplerState PointClampped : register(s16);
SamplerState LinearClampped : register(s17);

#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#include "UHLightCommon.hlsli"
#include "UHSphericalHamonricCommon.hlsli"

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

float ReceiveLightMask(uint ReceiveLightBits, uint InBits)
{
    return (ReceiveLightBits & InBits) ? 1.0f : 0.0f;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
	{
		return;
	}

    uint2 PixelCoord = DTid.xy;
    float2 UV = (PixelCoord + 0.5f) * GResolution.zw;
    float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV, 0).r;
    float4 CurrSceneData = SceneResult[PixelCoord];
    IndirectOcclusionResult[PixelCoord] = 1;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
    if (Depth == 0.0f || !HasLighting())
	{
		return;
	}
    
    // fetch data for light sampling
    float4 Diffuse = SceneBuffers[0].SampleLevel(LinearClampped, UV, 0);
    float3 WorldNormal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, UV, 0).xyz);
    float4 Specular = SceneBuffers[2].SampleLevel(LinearClampped, UV, 0);
    float AttenNoise = GetAttenuationNoise(PixelCoord);
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(PixelCoord + 0.5f), Depth);
    
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Diffuse.rgb;
    LightInfo.Specular = Specular;
    LightInfo.Normal = WorldNormal;
    LightInfo.WorldPos = WorldPos;
    LightInfo.AttenNoise = AttenNoise;
    LightInfo.SpecularNoise = AttenNoise * lerp(0.5f, 0.02f, Specular.a);
    
    uint2 ShadowCoord = PixelCoord * GShadowResolution.xy * GResolution.zw;
    uint ReceiveLightBits = RTReceiveLightBits[ShadowCoord];
    
    float3 DirectResult = 0;
    uint TraceCount = 0;
    uint SoftShadowCount = 0;
    
    // ------------------------------------------------------------------------------------------ direct light sampling
    for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
    {
        if (UHDirLights[Ldx].bIsEnabled)
        {
            if (SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                LightInfo.ShadowMask = RTShadowResult.SampleLevel(LinearClampped, float3(UV, SoftShadowCount++), 0);
            }
            else
            {
                LightInfo.ShadowMask = ReceiveLightMask(ReceiveLightBits, 1u << TraceCount);
            }
            
            DirectResult += CalculateDirLight(UHDirLights[Ldx], LightInfo);
            TraceCount++;
        }
    }
    
    // fetch tiles for positional lights
    uint TileX = CoordToTileX(PixelCoord.x);
    uint TileY = CoordToTileY(PixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
    
    uint ClosestPointLightIndex = GetClosestPointLightIndex(PointLightList, TileIndex, WorldPos);
    UHLOOP
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
        TileOffset += 4;
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        if (PointLight.bIsEnabled)
        {
            // fetch soft shadow or only the mask
            if (ClosestPointLightIndex == PointLightIdx && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                LightInfo.ShadowMask = RTShadowResult.SampleLevel(LinearClampped, float3(UV, SoftShadowCount++), 0);
            }
            else
            {
                LightInfo.ShadowMask = ReceiveLightMask(ReceiveLightBits, 1u << TraceCount);
            }
            
            DirectResult += CalculatePointLight(PointLight, LightInfo);
            TraceCount++;
        }
    }
    
    TileOffset = GetSpotLightOffset(TileIndex);
    uint SpotLightCount = SpotLightList.Load(TileOffset);
    TileOffset += 4;
    
    uint ClosestSpotLightIndex = GetClosestSpotLightIndex(SpotLightList, TileIndex, WorldPos);
    UHLOOP
    for (Ldx = 0; Ldx < SpotLightCount; Ldx++)
    {
        uint SpotLightIdx = SpotLightList.Load(TileOffset);
        TileOffset += 4;
        
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        if (SpotLight.bIsEnabled)
        {
            // fetch soft shadow or only the mask
            if (ClosestSpotLightIndex == SpotLightIdx && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                LightInfo.ShadowMask = RTShadowResult.SampleLevel(LinearClampped, float3(UV, SoftShadowCount++), 0);
            }
            else
            {
                LightInfo.ShadowMask = ReceiveLightMask(ReceiveLightBits, 1u << TraceCount);
            }
            
            DirectResult += CalculateSpotLight(SpotLight, LightInfo);
            TraceCount++;
        }
    }
    
    // ------------------------------------------------------------------------------------------ indirect light sampling
    float3 ILResult = 0;
    
    // SH9
    float Occlusion = 0;
    float3 SkyLight = ShadeSH9(Diffuse.rgb, float4(WorldNormal, 1.0f), 1.0f);
    
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
  
            // @TODO: soft occlusion
            float SoftOcclusion = IL.a;
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