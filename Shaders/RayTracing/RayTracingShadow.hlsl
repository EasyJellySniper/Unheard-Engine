#define UHDIRLIGHT_BIND t3
#define UHPOINTLIGHT_BIND t4
#define UHSPOTLIGHT_BIND t5
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);

// this pass will store result as a float2 buffer (max hit distance, attenuation)
// in soften pass, it will output to corresponding RT shadow targets
RWTexture2D<float2> OutShadowResult : register(u2);

ByteAddressBuffer PointLightList : register(t6);
ByteAddressBuffer PointLightListTrans : register(t7);
ByteAddressBuffer SpotLightList : register(t8);
ByteAddressBuffer SpotLightListTrans : register(t9);

Texture2D MixedMipTexture : register(t10);
Texture2D MixedDepthTexture : register(t11);
Texture2D MixedVertexNormalTexture : register(t12);
SamplerState PointSampler : register(s13);
SamplerState LinearSampler : register(s14);

// both opaque and translucent shadow are traced in this function
void TraceShadow(uint2 PixelCoord, float2 ScreenUV, float MipRate, float MipLevel)
{
    // The MixedDepthTexture contains both opaque/translucent depth
    float SceneDepth = MixedDepthTexture.SampleLevel(PointSampler, ScreenUV, 0).r;

	UHBRANCH
    if (SceneDepth == 0.0f)
    {
        // early out if no depth
		return;
    }

	// reconstruct world position and get world normal
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth);

	// reconstruct normal, simply sample from the texture
    // likewise,  The MixedVertexNormalTexture contains both opaque/translucent's vertex normal
    float4 VertexNormalData = MixedVertexNormalTexture.SampleLevel(PointSampler, ScreenUV, 0);
    float3 WorldNormal = DecodeNormal(VertexNormalData.xyz);
	
	// ------------------------------------------------------------------------------------------ Directional Light Tracing
	// give a little gap for preventing self-shadowing, along the vertex normal direction
	// distant pixel needs higher TMin
    float Gap = lerp(0.01f, 0.05f, saturate(MipRate * RT_MIPRATESCALE));
	float MaxDist = 0;
	float Atten = 0.0f;
    bool bAlreadyHasShadow = false;

	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];
        UHBRANCH
        if (!DirLight.bIsEnabled)
        {
            continue;
        }

		RayDesc ShadowRay = (RayDesc)0;
		ShadowRay.Origin = WorldPos + WorldNormal * Gap;
		ShadowRay.Direction = -DirLight.Dir;

		ShadowRay.TMin = Gap;
        ShadowRay.TMax = UHDirectionalShadowRayTMax;

		UHDefaultPayload Payload = (UHDefaultPayload)0;
        if (!bAlreadyHasShadow)
        {
            Payload.MipLevel = MipLevel;
            TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);
        }

		// store the max hit T to the result, system will perform PCSS later
		// also output shadow attenuation, hit alpha and ndotl are considered
        float NdotL = saturate(dot(-DirLight.Dir, WorldNormal)) * saturate(DirLight.Color.a);
		if (Payload.IsHit() && !bAlreadyHasShadow)
		{
			MaxDist = max(MaxDist, Payload.HitT);
            Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
            bAlreadyHasShadow = true;
        }
		else
        {
			// add attenuation by ndotl
            Atten += NdotL;
        }
    }
    
    // evaluate if it's translucent or not from VertexNormalData.w
    bool bIsTranslucent = VertexNormalData.w > 0;
    
	// ------------------------------------------------------------------------------------------ Point Light Tracing
    uint2 TileCoordinate = PixelCoord.xy;
    uint TileX = TileCoordinate.x / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileY = TileCoordinate.y / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileIndex = TileX + TileY * UHLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint LightCount = (bIsTranslucent) ? PointLightListTrans.Load(TileOffset) : PointLightList.Load(TileOffset);
    TileOffset += 4;
	
	// need to calculate light attenuation to lerp shadow attenuation 
    float3 LightToWorld;
    float AttenNoise = GetAttenuationNoise(TileCoordinate);
    float LightAtten;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint PointLightIdx = (bIsTranslucent) ? PointLightListTrans.Load(TileOffset) : PointLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        UHBRANCH
        if (!PointLight.bIsEnabled)
        {
            continue;
        }
        LightToWorld = WorldPos - PointLight.Position;
		
		// point only needs to be traced by the length of LightToWorld
        RayDesc ShadowRay = (RayDesc)0;
        ShadowRay.Origin = WorldPos + WorldNormal * Gap;
        ShadowRay.Direction = -normalize(LightToWorld);
        ShadowRay.TMin = Gap;
        ShadowRay.TMax = length(LightToWorld);
		
		// do not trace out-of-range pixel
        if (ShadowRay.TMax > PointLight.Radius)
        {
            continue;
        }
		
        // calc light attenuation for this point light
        LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        
        UHDefaultPayload Payload = (UHDefaultPayload)0;
        if (!bAlreadyHasShadow)
        {
            Payload.MipLevel = MipLevel;
            TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);
        }
		
        float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal)) * saturate(PointLight.Color.a) * LightAtten;
        if (Payload.IsHit() && !bAlreadyHasShadow)
        {
            MaxDist = max(MaxDist, Payload.HitT);
            Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
        }
        else
        {
			// add attenuation by ndotl
            Atten += NdotL;
        }
    }
	
	
	// ------------------------------------------------------------------------------------------ Spot Light Tracing
    TileOffset = GetSpotLightOffset(TileIndex);
    LightCount = (bIsTranslucent) ? SpotLightListTrans.Load(TileOffset) : SpotLightList.Load(TileOffset);
    TileOffset += 4;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint SpotLightIdx = (bIsTranslucent) ? SpotLightListTrans.Load(TileOffset) : SpotLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        UHBRANCH
        if (!SpotLight.bIsEnabled)
        {
            continue;
        }
        LightToWorld = WorldPos - SpotLight.Position;
		
		// point only needs to be traced by the length of LightToWorld
        RayDesc ShadowRay = (RayDesc) 0;
        ShadowRay.Origin = WorldPos + WorldNormal * Gap;
        ShadowRay.Direction = -SpotLight.Dir;
        ShadowRay.TMin = Gap;
        ShadowRay.TMax = length(LightToWorld);
		
		// do not trace out-of-range pixel
        float Rho = acos(dot(normalize(LightToWorld), SpotLight.Dir));
        if (ShadowRay.TMax > SpotLight.Radius || Rho > SpotLight.Angle)
        {
            continue;
        }
		
        UHDefaultPayload Payload = (UHDefaultPayload) 0;
        if (!bAlreadyHasShadow)
        {
            Payload.MipLevel = MipLevel;
            TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);
        }
        
        // calc light attenuation for this point light
        LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        
        // squared spot angle attenuation
        float SpotFactor = (cos(Rho) - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
        SpotFactor = saturate(SpotFactor);
        LightAtten *= SpotFactor * SpotFactor;
		
        float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal)) * saturate(SpotLight.Color.a) * LightAtten;
        if (Payload.IsHit() && !bAlreadyHasShadow)
        {
            MaxDist = max(MaxDist, Payload.HitT);
            Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
        }
        else
        {
			// add attenuation by ndotl
            Atten += NdotL;
        }
    }
	
	// saturate the attenuation and output
    Atten = saturate(Atten);
    OutShadowResult[PixelCoord] = float2(MaxDist, Atten);
}

[shader("raygeneration")]
void RTShadowRayGen() 
{
    // the tracing could be half-sized, but now the buffer is always the same resolution as rendering
    // so need to calculate proper pixel coordinate here
	uint2 PixelCoord = DispatchRaysIndex().xy * UHResolution.xy * RTShadowResolution.zw;
    OutShadowResult[PixelCoord] = float2(0.0f, 1.0f);
	
	// early return if no lights
	UHBRANCH
	if (!HasLighting())
	{
		return;
	}

    // to UV
	float2 ScreenUV = (PixelCoord + 0.5f) * UHResolution.zw;

	// calculate mip level before ray tracing kicks off
    float MipRate = MixedMipTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;
	float MipLevel = max(0.5f * log2(MipRate * MipRate), 0);

    // trace shadow just once, it will take care opaque/translucent tracing at the same time
	TraceShadow(PixelCoord, ScreenUV, MipRate, MipLevel);

    // if it's half-sized tracing, fill the empty pixels on right and bottom
    if (UHResolution.x != RTShadowResolution.x)
    {
        int Dx[3] = { 1,0,1 };
        int Dy[3] = { 0,1,1 };
        for (int I = 0; I < 3; I++)
        {
            int2 Pos = PixelCoord + int2(Dx[I], Dy[I]);
            Pos = min(Pos, UHResolution.xy - 1);
            OutShadowResult[Pos] = OutShadowResult[PixelCoord];
        }
    }
}

[shader("miss")]
void RTShadowMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}