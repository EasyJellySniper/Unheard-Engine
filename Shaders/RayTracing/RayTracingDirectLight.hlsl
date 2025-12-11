// RayTracingDirectLight.hlsl - Shader for opaque direct light tracing
#define UHGBUFFER_BIND t4
#define UHDIRLIGHT_BIND t5
#define UHPOINTLIGHT_BIND t6
#define UHSPOTLIGHT_BIND t7
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);

// this shader will calculate lighting based on ray tracing shadow results
// and also store the hit distance for later use
RWTexture2D<float3> OutLightResult : register(u2);
RWTexture2D<float> OutShadowHitDistance : register(u3);

ByteAddressBuffer PointLightList : register(t8);
ByteAddressBuffer SpotLightList : register(t9);

Texture2D SceneMipTexture : register(t10);

SamplerState PointClampped : register(s11);
SamplerState LinearClampped : register(s12);

void TraceDirectLight(uint2 PixelCoord, uint2 OutputCoord, float2 ScreenUV, float MipRate, float MipLevel)
{
    float SceneDepth = SceneBuffers[3].SampleLevel(PointClampped, ScreenUV, 0).r;

	UHBRANCH
    if (SceneDepth == 0.0f)
    {
        // early out if no depth
		return;
    }

	// reconstruct world position and get world normal
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);

	// reconstruct normal, simply sample from the texture
    float3 WorldNormal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, ScreenUV, 0).xyz);
	
	// ------------------------------------------------------------------------------------------ Directional Light Tracing
	// give a little gap for preventing self-shadowing, along the vertex normal direction
	// distant pixel needs higher TMin
    float Gap = lerp(GRTGapMin, GRTGapMax, saturate(MipRate * RT_MIPRATESCALE));

    // trace shadow and calculate lights at the same time    
    // also store the max hit distance
    float3 LightResult = 0;
    float MaxHitDistance = 0;
    
    float4 Diffuse = SceneBuffers[0].SampleLevel(LinearClampped, ScreenUV, 0);
    float4 Specular = SceneBuffers[2].SampleLevel(LinearClampped, ScreenUV, 0);
    float AttenNoise = GetAttenuationNoise(PixelCoord);
    
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Diffuse.rgb;
    LightInfo.Specular = Specular;
    LightInfo.Normal = WorldNormal;
    LightInfo.WorldPos = WorldPos;
    LightInfo.AttenNoise = AttenNoise;
    LightInfo.SpecularNoise = AttenNoise * lerp(0.5f, 0.02f, Specular.a);

	for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];
        UHBRANCH
        if (DirLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            
            TraceDiretionalShadow(TLAS, DirLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            LightInfo.ShadowMask = Atten;
            LightResult += CalculateDirLight(DirLight, LightInfo);
            MaxHitDistance = max(MaxHitDistance, HitDist);
        }
    }
    
	// ------------------------------------------------------------------------------------------ Point Light Tracing
    uint2 TileCoordinate = PixelCoord.xy;
    uint TileX = CoordToTileX(TileCoordinate.x);
    uint TileY = CoordToTileY(TileCoordinate.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint LightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
	
    float3 LightToWorld;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        UHBRANCH
        if (PointLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            
            TracePointShadow(TLAS, PointLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            LightInfo.ShadowMask = Atten;
            LightResult += CalculatePointLight(PointLight, LightInfo);
            MaxHitDistance = max(MaxHitDistance, HitDist);
        }
    }
	
	// ------------------------------------------------------------------------------------------ Spot Light Tracing
    TileOffset = GetSpotLightOffset(TileIndex);
    LightCount = SpotLightList.Load(TileOffset);
    TileOffset += 4;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint SpotLightIdx = SpotLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        UHBRANCH
        if (SpotLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            
            TraceSpotShadow(TLAS, SpotLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            LightInfo.ShadowMask = Atten;
            LightResult += CalculateSpotLight(SpotLight, LightInfo);
            MaxHitDistance = max(MaxHitDistance, HitDist);
        }
    }
	
    OutLightResult[OutputCoord] = LightResult;
    OutShadowHitDistance[OutputCoord] = MaxHitDistance;
}

[shader("raygeneration")]
void RTDirectLightRayGen() 
{
    uint2 OutputCoord = DispatchRaysIndex().xy;
    OutLightResult[OutputCoord] = 0;
    OutShadowHitDistance[OutputCoord] = 0;
	
	// early return if no lights
	UHBRANCH
	if (!HasLighting())
	{
		return;
	}

    // PixelCoord to UV
    uint2 PixelCoord = OutputCoord * GResolution.xy * GDirectLightResolution.zw;
	float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;

	// calculate mip level before ray tracing kicks off
    float MipRate = SceneMipTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;

    // trace direct light
    TraceDirectLight(PixelCoord, OutputCoord, ScreenUV, MipRate, MipLevel);
}

[shader("miss")]
void RTDirectLightMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}