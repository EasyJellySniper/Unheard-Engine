#define UHDIRLIGHT_BIND t1
#define UHPOINTLIGHT_BIND t2
#define UHGBUFFER_BIND t3
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u4);
Texture2D RTDirShadow : register(t5);
Texture2D RTPointShadow : register(t6);
ByteAddressBuffer PointLightList : register(t7);
SamplerState LinearClampped : register(s8);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= UHResolution.x || DTid.y >= UHResolution.y)
	{
		return;
	}

    float2 UV = (DTid.xy + 0.5f) * UHResolution.zw;
	float Depth = SceneBuffers[3].SampleLevel(LinearClampped, UV, 0).r;
    float3 CurrSceneColor = SceneResult[DTid.xy].rgb;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
    if (Depth == 0.0f || (UHNumDirLights == 0 && UHNumPointLights == 0))
	{
		return;
	}

	// reconstruct world position
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(DTid.xy + 0.5f), Depth);

	// get diffuse color, a is occlusion
	float4 Diffuse = SceneBuffers[0].SampleLevel(LinearClampped, UV, 0);

	// unpack normal from [0,1] to [-1,1]
    float3 Normal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, UV, 0).xyz);

	// get specular color, a is roughness
	float4 Specular = SceneBuffers[2].SampleLevel(LinearClampped, UV, 0);
    
	float3 Result = 0;

	// ------------------------------------------------------------------------------------------ dir lights accumulation
	float DirShadowMask = 1.0f;
#if WITH_RTSHADOWS
	DirShadowMask = RTDirShadow.SampleLevel(LinearClampped, UV, 0).r;
#endif


    UHLightInfo LightInfo;
    LightInfo.Diffuse = Diffuse.rgb;
    LightInfo.Specular = Specular;
    LightInfo.Normal = Normal;
    LightInfo.WorldPos = WorldPos;
    LightInfo.ShadowMask = DirShadowMask;
	
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
        UHDirectionalLight DirLight = UHDirLights[Ldx];
        LightInfo.LightColor = DirLight.Color.rgb;
        LightInfo.LightDir = DirLight.Dir;
        Result += LightBRDF(LightInfo);
    }
	
    // ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = DTid.x / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileY = DTid.y / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileIndex = TileX + TileY * UHLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint TileCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
	
    float3 WorldToLight;
    float LightAtten;
    float AttenNoise = lerp(-0.01f, 0.01f, CoordinateToHash(DTid.xy));
    
    float PointShadowMask = 1.0f;
#if WITH_RTSHADOWS
	PointShadowMask = RTPointShadow.SampleLevel(LinearClampped, UV, 0).r;
#endif
    
    UHLOOP
    for (Ldx = 0; Ldx < TileCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        LightInfo.LightColor = PointLight.Color.rgb;
		
        WorldToLight = WorldPos - PointLight.Position;
        LightInfo.LightDir = normalize(WorldToLight);
		
		// square distance attenuation, apply attenuation noise to reduce color banding
        LightAtten = 1.0f - saturate(length(WorldToLight) / PointLight.Radius) + AttenNoise;
        LightAtten *= LightAtten;
        LightInfo.ShadowMask = LightAtten * PointShadowMask;
		
        Result += LightBRDF(LightInfo);
        TileOffset += 4;
    }

	// indirect light accumulation
    Result += LightIndirect(Diffuse.rgb, Normal, Diffuse.a);

    SceneResult[DTid.xy] = float4(CurrSceneColor + Result, 0);
}