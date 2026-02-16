// Light pass shader
RWTexture2D<float4> SceneResult : register(u1);

#define UHGBUFFER_BIND t2
#define UHDIRLIGHT_BIND t3
#define UHPOINTLIGHT_BIND t4
#define UHSPOTLIGHT_BIND t5
ByteAddressBuffer PointLightList : register(t6);
ByteAddressBuffer SpotLightList : register(t7);
#define SH9_BIND t8

Texture2DArray<float> RTShadowResult : register(t9);
Texture2D<uint> RTReceiveLightBits : register(t10);

#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#include "UHLightCommon.hlsli"
#include "UHSphericalHamonricCommon.hlsli"

Texture2D RTIndirectDiffuse : register(t11);
Texture2D RTIndirectOcclusion : register(t12);

SamplerState PointClampped : register(s13);
SamplerState LinearClampped : register(s14);

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
    float3 IndirectResult = 0;
    
    // SH9
    float3 SkyLight = ShadeSH9(Diffuse.rgb, float4(WorldNormal, 1.0f), 1.0f);
    
    UHBRANCH
    if (GSystemRenderFeature & UH_RT_INDIRECTLIGHT)
    {
        // sample indirect diffuse and occlusion
        float3 ILDiffuse = RTIndirectDiffuse.SampleLevel(LinearClampped, UV, 0).rgb;
        IndirectResult += ILDiffuse.rgb * Diffuse.rgb;
    
        float ILOcclusion = RTIndirectOcclusion.SampleLevel(LinearClampped, UV, 0).r;
        IndirectResult += SkyLight * min(Diffuse.a, ILOcclusion);
    }
    else
    {
        IndirectResult += SkyLight * Diffuse.a;
    }
    
    // accumulate results to scene
    SceneResult[PixelCoord] = float4(CurrSceneData.rgb + DirectResult + IndirectResult, CurrSceneData.a);
}