#ifndef UHRTCOMMOM_H
#define UHRTCOMMOM_H

#include "../UHCommon.hlsli"
#define RT_MIPRATESCALE 100.0f

struct UHDefaultPayload
{
	bool IsHit() 
	{ 
		return HitT > 0; 
	}

	float HitT;
	float MipLevel;
	float HitAlpha;
};

RayDesc GenerateCameraRay(uint2 ScreenPos)
{
	// generate a ray to far plane, since infinite reversed z is used, can't assign 0 to depth parameter
	// simply give a tiny value
	float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(ScreenPos + 0.5f), 0.001f, true);

	RayDesc CameraRay;
	CameraRay.Origin = UHCameraPos;
	CameraRay.Direction = normalize(WorldPos - UHCameraPos);
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
	CameraRay.Origin = UHCameraPos;
	CameraRay.Direction = normalize(WorldPos - UHCameraPos);
	CameraRay.TMin = 0.0f;
	CameraRay.TMax = float(1 << 20);

	return CameraRay;
}

#endif