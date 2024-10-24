#define UHDIRLIGHT_BIND t1
#define UHPOINTLIGHT_BIND t2
#define UHSPOTLIGHT_BIND t3
#define UHGBUFFER_BIND t4
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"
#include "UHLightCommon.hlsli"

#define SH9_BIND t9
#include "UHSphericalHamonricCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u5);
Texture2D ScreenShadowTexture : register(t6);
ByteAddressBuffer PointLightList : register(t7);
ByteAddressBuffer SpotLightList : register(t8);
SamplerState PointClampped : register(s10);
SamplerState LinearClampped : register(s11);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
	{
		return;
	}

    float2 UV = (DTid.xy + 0.5f) * GResolution.zw;
	float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV, 0).r;
    float4 CurrSceneData = SceneResult[DTid.xy];

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
    if (Depth == 0.0f || !HasLighting())
	{
		return;
	}

	// reconstruct world position
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(DTid.xy + 0.5f), Depth);

	// get diffuse color, a is occlusion
	float4 Diffuse = SceneBuffers[0].SampleLevel(PointClampped, UV, 0);

	// unpack normal from [0,1] to [-1,1]
    float3 Normal = DecodeNormal(SceneBuffers[1].SampleLevel(PointClampped, UV, 0).xyz);

	// get specular color, a is smoothness
	float4 Specular = SceneBuffers[2].SampleLevel(PointClampped, UV, 0);
    
	float3 Result = 0;
    
    // Noise (dithering) to reduce color banding
    float AttenNoise = GetAttenuationNoise(DTid.xy);
    
	// ------------------------------------------------------------------------------------------ dir lights accumulation
    float ShadowMask = ScreenShadowTexture.SampleLevel(LinearClampped, UV, 0).r;

    UHLightInfo LightInfo;
    LightInfo.Diffuse = Diffuse.rgb;
    LightInfo.Specular = Specular;
    LightInfo.Normal = Normal;
    LightInfo.WorldPos = WorldPos;
    LightInfo.ShadowMask = ShadowMask;
    LightInfo.SpecularNoise = AttenNoise * 0.1f;
	
	for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
	{
        UHDirectionalLight DirLight = UHDirLights[Ldx];
        UHBRANCH
        if (!DirLight.bIsEnabled)
        {
            continue;
        }
        
        LightInfo.LightColor = DirLight.Color.rgb;
        LightInfo.LightDir = DirLight.Dir;
        Result += LightBRDF(LightInfo);
    }
	
    // ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = CoordToTileX(DTid.x);
    uint TileY = CoordToTileY(DTid.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
	
    float3 LightToWorld;
    float LightAtten;
    
    UHLOOP
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
        TileOffset += 4;
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        UHBRANCH
        if (!PointLight.bIsEnabled)
        {
            continue;
        }
        LightInfo.LightColor = PointLight.Color.rgb;
		
        LightToWorld = WorldPos - PointLight.Position;
        LightInfo.LightDir = normalize(LightToWorld);
		
		// square distance attenuation, apply attenuation noise to reduce color banding
        LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        LightInfo.ShadowMask = LightAtten * ShadowMask;
		
        Result += LightBRDF(LightInfo);
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
        UHBRANCH
        if (!SpotLight.bIsEnabled)
        {
            continue;
        }
        
        LightInfo.LightColor = SpotLight.Color.rgb;
        LightInfo.LightDir = SpotLight.Dir;
        LightToWorld = WorldPos - SpotLight.Position;
        
        // squared distance attenuation
        LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        
        // squared spot angle attenuation
        float Rho = dot(SpotLight.Dir, normalize(LightToWorld));
        float SpotFactor = (Rho - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
        SpotFactor = saturate(SpotFactor);
        
        LightAtten *= SpotFactor * SpotFactor;
        LightInfo.ShadowMask = LightAtten * ShadowMask;
		
        Result += LightBRDF(LightInfo);
    }

	// indirect light accumulation
    Result += ShadeSH9(Diffuse.rgb, float4(Normal, 1.0f), Diffuse.a);

    SceneResult[DTid.xy] = float4(CurrSceneData.rgb + Result, CurrSceneData.a);
}