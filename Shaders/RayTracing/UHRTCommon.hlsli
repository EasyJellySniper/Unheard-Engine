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
#define PAYLOAD_ISINDIRECTLIGHT 1 << 3

// perf hack mip bias for RT, it also reduces the noise from sharp textures
static const float GRTMipBias = 2.0f;
static const uint GMaxPointSpotLightPerInstance = 16;
static const int GRTMaxDirLight = 2;
static const float GRTGapMin = 0.005f;
static const float GRTGapMax = 0.05f;
static const uint GRTMinimalHitGroupOffset = 1;
static const uint GRTShadowMissShaderIndex = 0;
static const uint GRTDefaultMissShaderIndex = 1;

struct UHInstanceLights
{
    uint PointLightIndices[GMaxPointSpotLightPerInstance];
    uint SpotLightIndices[GMaxPointSpotLightPerInstance];
};

// struct has to be the same as the c++ define in RTShaderDefines.h
struct UHMinimalPayload
{
    bool IsHit()
    {
        return HitT > 0;
    }
    
    float HitT;
    float MipLevel;
    float HitAlpha;
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
        HitMaterialNormal = SrcPayload.HitMaterialNormal;
        HitVertexNormal = SrcPayload.HitVertexNormal;
        HitSpecular = SrcPayload.HitSpecular;
        HitEmissive = SrcPayload.HitEmissive;
        HitRefractOffset = SrcPayload.HitRefractOffset;
        
        HitInstanceIndex = SrcPayload.HitInstanceIndex;
        RayDir = SrcPayload.RayDir;
        FresnelFactor = SrcPayload.FresnelFactor;
    }

    // UHMinimalPayload
	float HitT;
	float MipLevel;
	float HitAlpha;
    
    // UHIndirectPayload
	// HLSL will pad bool to 4 bytes, so using uint here anyway
	// Could pack more data to this variable in the future
    uint PayloadData;
    float4 HitDiffuse;
    float3 HitVertexNormal;
    uint HitInstanceIndex;
    
    float3 HitMaterialNormal;
    float4 HitSpecular;
	float3 HitEmissive;
    float2 HitRefractOffset;
	
    uint CurrentRecursion;
    float3 RayDir;
    float FresnelFactor;
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
    , inout float Atten, inout float HitDist)
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
        Atten = 0.0f;
        return false;
    }
    
    RayDesc ShadowRay = (RayDesc)0;
    ShadowRay.Origin = WorldPos + WorldNormal * Gap;
    ShadowRay.Direction = -DirLight.Dir;

    ShadowRay.TMin = Gap;
    ShadowRay.TMax = GDirectionalShadowRayTMax;

    UHMinimalPayload Payload = (UHMinimalPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, GRTMinimalHitGroupOffset, 0, GRTShadowMissShaderIndex, ShadowRay, Payload);

	// output both attenuation and hit distance
    if (Payload.IsHit())
    {
        HitDist = Payload.HitT;
    }
    else
    {
        HitDist = 0.0f;
    }
    
    // set attenuation as 1 - HitAlpha regardless if it's hitting
    // as the any hit shader can still have translucent instances hit and HitAlpha stored
    Atten = 1 - Payload.HitAlpha;
    
    return true;
}

bool TracePointShadow(RaytracingAccelerationStructure TLAS
    , UHPointLight PointLight
    , float3 WorldPos
    , float3 WorldNormal
    , float Gap
    , float MipLevel
    , inout float Atten, inout float HitDist)
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
        Atten = 0.0f;
        return false;
    }
        
    float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal));
    if (NdotL == 0.0f)
    {
        // no need to trace for backface
        Atten = 0.0f;
        return false;
    }
        
    UHMinimalPayload Payload = (UHMinimalPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, GRTMinimalHitGroupOffset, 0, GRTShadowMissShaderIndex, ShadowRay, Payload);
		
    if (Payload.IsHit())
    {
        HitDist = Payload.HitT;
    }
    else
    {
        HitDist = 0.0f;
    }
    
    // set attenuation as 1 - HitAlpha regardless if it's hitting
    // as the any hit shader can still have translucent instances hit and HitAlpha stored
    Atten = 1 - Payload.HitAlpha;
    
    return true;
}

bool TraceSpotShadow(RaytracingAccelerationStructure TLAS
    , UHSpotLight SpotLight
    , float3 WorldPos
    , float3 WorldNormal
    , float Gap
    , float MipLevel
    , inout float Atten, inout float HitDist)
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
        Atten = 0.0f;
        return false;
    }
        
    float NdotL = saturate(dot(ShadowRay.Direction, WorldNormal));
    if (NdotL == 0.0f)
    {
        // no need to trace for backface
        Atten = 0.0f;
        return false;
    }
		
    UHMinimalPayload Payload = (UHMinimalPayload) 0;
    Payload.MipLevel = MipLevel;
    TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, GRTMinimalHitGroupOffset, 0, GRTShadowMissShaderIndex, ShadowRay, Payload);
		
    if (Payload.IsHit())
    {
        HitDist = Payload.HitT;
    }
    else
    {
        HitDist = 0.0f;
    }
    
    // set attenuation as 1 - HitAlpha regardless if it's hitting
    // as the any hit shader can still have translucent instances hit and HitAlpha stored
    Atten = 1 - Payload.HitAlpha;
    
    return true;
}

#endif