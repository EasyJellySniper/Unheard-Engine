#define UHDIRLIGHT_BIND t1
#define UHPOINTLIGHT_BIND t2
#define UHSPOTLIGHT_BIND t3
#define UHGBUFFER_BIND t4
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#include "UHLightCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u5);
Texture2D ScreenShadowTexture : register(t6);
Texture2D IndirectLightResult : register(t7);
ByteAddressBuffer PointLightList : register(t8);
ByteAddressBuffer SpotLightList : register(t9);
SamplerState LinearClampped : register(s10);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
	{
		return;
	}

    uint2 PixelCoord = DTid.xy;
    float2 UV = (DTid.xy + 0.5f) * GResolution.zw;
    float Depth = SceneBuffers[3][PixelCoord].r;
    float4 CurrSceneData = SceneResult[PixelCoord];

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
    if (Depth == 0.0f || !HasLighting())
	{
		return;
	}

	// reconstruct world position
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(DTid.xy + 0.5f), Depth);

	// get diffuse color, a is occlusion
    float4 Diffuse = SceneBuffers[0][PixelCoord];

	// unpack normal from [0,1] to [-1,1]
    float3 Normal = DecodeNormal(SceneBuffers[1][PixelCoord].xyz);

	// get specular color, a is smoothness
    float4 Specular = SceneBuffers[2][PixelCoord];
    
	float3 Result = 0;
    
    // Shadow mask fetch
    float ShadowMask = ScreenShadowTexture.SampleLevel(LinearClampped, UV, 0).r;
    
    // Noise (dithering) to reduce color banding
    float AttenNoise = GetAttenuationNoise(DTid.xy);
    
	// ------------------------------------------------------------------------------------------ dir lights accumulation
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Diffuse.rgb;
    LightInfo.Specular = Specular;
    LightInfo.Normal = Normal;
    LightInfo.WorldPos = WorldPos;
    LightInfo.ShadowMask = ShadowMask;
    LightInfo.AttenNoise = AttenNoise;
    LightInfo.SpecularNoise = AttenNoise * lerp(0.5f, 0.02f, Specular.a);
	
	for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
	{
        UHDirectionalLight DirLight = UHDirLights[Ldx];
        Result += CalculateDirLight(DirLight, LightInfo);
    }
	
    // ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = CoordToTileX(DTid.x);
    uint TileY = CoordToTileY(DTid.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
    
    UHLOOP
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
        TileOffset += 4;
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        Result += CalculatePointLight(PointLight, LightInfo);
    }
    
    // ------------------------------------------------------------------------------------------ spot lights accumulation
    // similar as the point light but giving it angle attenuation as well
    TileOffset = GetSpotLightOffset(TileIndex);
    uint SpotLightCount = SpotLightList.Load(TileOffset);
    TileOffset += 4;
    
    UHLOOP
    for (Ldx = 0; Ldx < SpotLightCount; Ldx++)
    {
        uint SpotLightIdx = SpotLightList.Load(TileOffset);
        TileOffset += 4;
        
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        Result += CalculateSpotLight(SpotLight, LightInfo);
    }
    
    // ------------------------------------------------------------------------------------------ indirect light sampling
    Result += IndirectLightResult.SampleLevel(LinearClampped, UV, 0).rgb;

    SceneResult[DTid.xy] = float4(CurrSceneData.rgb + Result, CurrSceneData.a);
}