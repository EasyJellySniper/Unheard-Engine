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
Texture2D MipTexture : register(t10);
Texture2D DepthTexture : register(t11);
Texture2D TranslucentDepthTexture : register(t12);
Texture2D VertexNormalTexture : register(t13);
Texture2D TranslucentVertexNormalTexture : register(t14);
SamplerState LinearSampler : register(s15);

static const float GTranslucentShadowCutoff = 0.2f;

// both opaque and translucent shadow are traced in this function
void TraceShadow(uint2 PixelCoord, float2 ScreenUV, float OpaqueDepth, float MipRate, float MipLevel, bool bIsTranslucent)
{
	UHBRANCH
    if (OpaqueDepth == 0.0f && !bIsTranslucent)
	{
		// early return if no opaque depth (no object)
		return;
	}
	
    float TranslucentDepth = bIsTranslucent ? TranslucentDepthTexture.SampleLevel(LinearSampler, ScreenUV, 0).r : 0.0f;

	UHBRANCH
    if (abs(OpaqueDepth - TranslucentDepth) < GTranslucentShadowCutoff && bIsTranslucent)
    {
		// when opaque and translucent depth are close, considering this pixel contains no translucent object, simply share the result from opaque and return
        return;
    }

	UHBRANCH
    if (TranslucentDepth == 0.0f && bIsTranslucent)
    {
		// early return if there is no translucent at all
        return;
    }

	// reconstruct world position and get world normal
    float Depth = bIsTranslucent ? TranslucentDepth : OpaqueDepth;
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, Depth);

	// reconstruct normal, simply sample from the texture
    float3 WorldNormal = bIsTranslucent ? DecodeNormal(TranslucentVertexNormalTexture.SampleLevel(LinearSampler, ScreenUV, 0).xyz)
		: DecodeNormal(VertexNormalTexture.SampleLevel(LinearSampler, ScreenUV, 0).xyz);

	
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
		ShadowRay.TMax = float(1 << 20);

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
	
	
	// ------------------------------------------------------------------------------------------ Point Light Tracing
    uint2 TileCoordinate = PixelCoord.xy * UHResolution.xy * UHShadowResolution.zw;
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
	uint2 PixelCoord = DispatchRaysIndex().xy;
    // clear attenuation as 1.0f in case of black edge
    OutShadowResult[PixelCoord] = float2(0.0f, 1.0f);
	
	// early return if no lights
	UHBRANCH
	if (UHNumDirLights == 0 && UHNumPointLights == 0)
	{
		return;
	}

	// to UV and get depth
	float2 ScreenUV = (PixelCoord + 0.5f) * UHShadowResolution.zw;
	float OpaqueDepth = DepthTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;

	// calculate mip level before ray tracing kicks off
	float MipRate = MipTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;
	float MipLevel = max(0.5f * log2(MipRate * MipRate), 0);

	// trace for translucent objs after opaque, the second OpaqueDepth isn't wrong since it will be compared in translucent function
	TraceShadow(PixelCoord, ScreenUV, OpaqueDepth, MipRate, MipLevel, false);
	TraceShadow(PixelCoord, ScreenUV, OpaqueDepth, MipRate, MipLevel, true);
}

[shader("miss")]
void RTShadowMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}