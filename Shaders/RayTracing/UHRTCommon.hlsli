#ifndef UHRTCOMMOM_H
#define UHRTCOMMOM_H

#include "../UHCommon.hlsli"
#define RT_MIPRATESCALE 100.0f

// payload data bits
#define PAYLOAD_ISREFLECTION 1 << 0
#define PAYLOAD_HITTRANSLUCENT 1 << 1
#define PAYLOAD_HITREFRACTION 1 << 2

struct UHRendererInstance
{
    // renderer index to lookup object constants
    uint RendererIndex;
    // mesh index to lookup mesh data
    uint MeshIndex;
    // num meshlets to dispatch
    uint NumMeshlets;
    // indice type
    uint IndiceType;
};

struct UHDefaultPayload
{
	bool IsHit() 
	{ 
		return HitT > 0; 
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
    float4 HitSpecular;
	float3 HitEmissive;
    float2 HitScreenUV;
	
	// for translucent
    float4 HitDiffuseTrans;
    float3 HitNormalTrans;
    float4 HitSpecularTrans;
	// .a will store the opacity, which used differently from the HitAlpha above!
    float4 HitEmissiveTrans;
    float2 HitScreenUVTrans;
    float3 HitWorldPosTrans;
    float2 HitRefractScale;
    float HitRefraction;
	
    uint IsInsideScreen;
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

#endif