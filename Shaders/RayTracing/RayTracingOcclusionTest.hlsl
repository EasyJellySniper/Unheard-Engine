#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWByteAddressBuffer OcclusionVisible : register(u2);

// assume it's tested with a half resolution
static const float GScale = 2;

[shader("raygeneration")]
void RTOcclusionTestRayGen()
{
	uint2 PixelCoord = DispatchRaysIndex().xy;

	// to UV
	float2 ScreenUV = (PixelCoord + 0.5f) * UHResolution.zw * GScale;
	RayDesc CameraRay = GenerateCameraRay_UV(ScreenUV);

	UHDefaultPayload Payload = (UHDefaultPayload)0;
	TraceRay(TLAS, 0, 0xff, 0, 0, 0, CameraRay, Payload);

	if (Payload.IsHit())
	{
		// simply shooting a ray from camera and finding the closest object
		// those closest objects are considered as visible, and the other which are failed with ray test are considered as occluded
		// the visibility will be reset in C++ side
		OcclusionVisible.Store(Payload.HitInstance * 4, 1);
	}
}