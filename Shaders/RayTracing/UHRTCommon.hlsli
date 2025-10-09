#ifndef UHRTCOMMOM_H
#define UHRTCOMMOM_H

#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "../UHLightCommon.hlsli"
#define RT_MIPRATESCALE 100.0f
#define RT_MATERIALDATA_SLOT 32

// payload data bits
#define PAYLOAD_ISREFLECTION 1 << 0
#define PAYLOAD_HITTRANSLUCENT 1 << 1
#define PAYLOAD_HITREFRACTION 1 << 2

// perf hack mip bias for RT, it also reduces the noise from sharp textures
static const float GRTMipBias = 2.0f;
static const uint GMaxPointSpotLightPerInstance = 16;

struct UHInstanceLights
{
    uint PointLightIndices[GMaxPointSpotLightPerInstance];
    uint SpotLightIndices[GMaxPointSpotLightPerInstance];
};

struct UHDefaultPayload
{
	bool IsHit() 
	{ 
		return HitT > 0; 
	}
    
    // copy function, not all parameters are copied
    void CopyFrom(UHDefaultPayload SrcPayload)
    {
        HitT = SrcPayload.HitT;
        MipLevel = SrcPayload.MipLevel;
        HitAlpha = SrcPayload.HitAlpha;
        PayloadData = SrcPayload.PayloadData;
        
        HitDiffuse = SrcPayload.HitDiffuse;
        HitNormal = SrcPayload.HitNormal;
        HitWorldNormal = SrcPayload.HitWorldNormal;
        HitSpecular = SrcPayload.HitSpecular;
        HitEmissive = SrcPayload.HitEmissive;
        HitScreenUV = SrcPayload.HitScreenUV;
        
        HitDiffuseTrans = SrcPayload.HitDiffuseTrans;
        HitNormalTrans = SrcPayload.HitNormalTrans;
        HitWorldNormalTrans = SrcPayload.HitWorldNormalTrans;
        HitSpecularTrans = SrcPayload.HitSpecularTrans;
        HitEmissiveTrans = SrcPayload.HitEmissiveTrans;
        HitScreenUVTrans = SrcPayload.HitScreenUVTrans;
        HitWorldPosTrans = SrcPayload.HitWorldPosTrans;
        HitRefractOffset = SrcPayload.HitRefractOffset;
        
        IsInsideScreen = SrcPayload.IsInsideScreen;
        HitInstanceIndex = SrcPayload.HitInstanceIndex;
        PackedData0 = SrcPayload.PackedData0;
        RayDir = SrcPayload.RayDir;
    }

	float HitT;
	float MipLevel;
	float HitAlpha;
	// HLSL will pad bool to 4 bytes, so using uint here anyway
	// Could pack more data to this variable in the future
    uint PayloadData;
	
	// for opaque
    float4 HitDiffuse;
    float3 HitNormal;
    float3 HitWorldNormal;
    float4 HitSpecular;
	float3 HitEmissive;
    float2 HitScreenUV;
	
	// for translucent
    float4 HitDiffuseTrans;
    float3 HitNormalTrans;
    float3 HitWorldNormalTrans;
    float4 HitSpecularTrans;
	// .a will store the opacity, which used differently from the HitAlpha above!
    float4 HitEmissiveTrans;
    float2 HitScreenUVTrans;
    float3 HitWorldPosTrans;
    float2 HitRefractOffset;
	
    uint IsInsideScreen;
    uint HitInstanceIndex;
    uint CurrentRecursion;
    // PackedData0, this stores hit world position and the fresnel factor now
    float4 PackedData0;
    float3 RayDir;
};

RayDesc GenerateCameraRay(uint2 ScreenPos)
{
	// generate a ray to far plane, since infinite reversed z is used, can't assign 0 to depth parameter
	// simply give a tiny value
	float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(ScreenPos + 0.5f), 0.001f, true);

	RayDesc CameraRay;
	CameraRay.Origin = GCameraPos;
	CameraRay.Direction = normalize(WorldPos - GCameraPos);
	CameraRay.TMin = 0.0f;
	CameraRay.TMax = float(1 << 20);

	return CameraRay;
}

RayDesc GenerateCameraRay_UV(float2 ScreenUV)
{
	// generate a ray to far plane, since infinite reversed z is used, can't assign 0 to depth parameter
	// simply give a tiny value
	float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, 0.001f, true);

	RayDesc CameraRay;
	CameraRay.Origin = GCameraPos;
	CameraRay.Direction = normalize(WorldPos - GCameraPos);
	CameraRay.TMin = 0.0f;
	CameraRay.TMax = float(1 << 20);

	return CameraRay;
}

bool TraceDiretionalShadow(RaytracingAccelerationStructure TLAS
    , UHDirectionalLight DirLight
    , float3 WorldPos
    , float3 WorldNormal
    , float Gap
    , float MipLevel
    , inout float Atten, inout float MaxDist)
{
	UHBRANCH
    if (!DirLight.bIsEnabled)
    {
        return false;
    }
        
    float NdotL = saturate(dot(-DirLight.Dir, WorldNormal));
    if (NdotL == 0.0f)
    {
        // no need to trace for backface
        return false;
    }
    NdotL *= saturate(DirLight.Color.a);

    RayDesc ShadowRay = (RayDesc)0;
    ShadowRay.Origin = WorldPos + WorldNormal * Gap;
    ShadowRay.Direction = -DirLight.Dir;

    ShadowRay.TMin = Gap;
    ShadowRay.TMax = GDirectionalShadowRayTMax;

    UHDefaultPayload Payload = (UHDefaultPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);

	// store the max hit T to the result, system will perform PCSS later
	// also output shadow attenuation, hit alpha and ndotl are considered
    if (Payload.IsHit())
    {
        MaxDist = max(MaxDist, Payload.HitT);
        Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
    }
    else
    {
		// add attenuation by ndotl
        Atten += NdotL;
    }
    
    return true;
}

bool TracePointShadow(RaytracingAccelerationStructure TLAS
    , UHPointLight PointLight
    , float3 WorldPos
    , float3 WorldNormal
    , float Gap
    , float MipLevel
    , inout float Atten, inout float MaxDist)
{
    UHBRANCH
    if (!PointLight.bIsEnabled)
    {
        return false;
    }
    float3 LightToWorld = WorldPos - PointLight.Position;
		
	// point only needs to be traced by the length of LightToWorld
    RayDesc ShadowRay = (RayDesc)0;
    ShadowRay.Origin = WorldPos + WorldNormal * Gap;
    ShadowRay.Direction = -normalize(LightToWorld);
    ShadowRay.TMin = Gap;
    ShadowRay.TMax = length(LightToWorld);
		
	// do not trace out-of-range pixel
    if (ShadowRay.TMax > PointLight.Radius)
    {
        return false;
    }
        
    float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal));
    if (NdotL == 0.0f)
    {
        // no need to trace for backface
        return false;
    }
    NdotL *= saturate(PointLight.Color.a);
        
    UHDefaultPayload Payload = (UHDefaultPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);
		
    if (Payload.IsHit())
    {
        MaxDist = max(MaxDist, Payload.HitT);
        Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
    }
    else
    {
		// add attenuation by ndotl
        Atten += NdotL;
    }
    
    return true;
}

bool TraceSpotShadow(RaytracingAccelerationStructure TLAS
    , UHSpotLight SpotLight
    , float3 WorldPos
    , float3 WorldNormal
    , float Gap
    , float MipLevel
    , inout float Atten, inout float MaxDist)
{
    UHBRANCH
    if (!SpotLight.bIsEnabled)
    {
        return false;
    }
    float3 LightToWorld = WorldPos - SpotLight.Position;
		
		// point only needs to be traced by the length of LightToWorld
    RayDesc ShadowRay = (RayDesc)0;
    ShadowRay.Origin = WorldPos + WorldNormal * Gap;
    ShadowRay.Direction = -SpotLight.Dir;
    ShadowRay.TMin = Gap;
    ShadowRay.TMax = length(LightToWorld);
		
		// do not trace out-of-range pixel
    float Rho = acos(dot(normalize(LightToWorld), SpotLight.Dir));
    if (ShadowRay.TMax > SpotLight.Radius || Rho > SpotLight.Angle)
    {
        return false;
    }
        
    float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal));
    if (NdotL == 0.0f)
    {
        // no need to trace for backface
        return false;
    }
    NdotL *= saturate(SpotLight.Color.a);
		
    UHDefaultPayload Payload = (UHDefaultPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);
		
    if (Payload.IsHit())
    {
        MaxDist = max(MaxDist, Payload.HitT);
        Atten = lerp(Atten + NdotL, Atten, Payload.HitAlpha);
    }
    else
    {
		// add attenuation by ndotl
        Atten += NdotL;
    }
    
    return true;
}

#endif