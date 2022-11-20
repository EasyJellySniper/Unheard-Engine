#define UHDIRLIGHT_BIND t3
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWTexture2D<float2> Result : register(u2);
Texture2D MipTexture : register(t4);
Texture2D NormalTexture : register(t5);
Texture2D DepthTexture : register(t6);
SamplerState PointSampler : register(s7);
SamplerState LinearSampler : register(s8);

[shader("raygeneration")]
void RTShadowRayGen() 
{
	uint2 PixelCoord = DispatchRaysIndex().xy;
	Result[PixelCoord] = 0;

	// to UV
	float2 ScreenUV = (PixelCoord + 0.5f) * UHShadowResolution.zw;

	float Depth = DepthTexture.SampleLevel(PointSampler, ScreenUV, 0).r;
	UHBRANCH
	if (UHNumDirLights == 0 || Depth == 0.0f)
	{
		return;
	}

	// reconstruct world position and get world normal
	float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, Depth);

	float3 WorldNormal = NormalTexture.SampleLevel(LinearSampler, ScreenUV, 0).xyz;
	WorldNormal = WorldNormal * 2.0f - 1.0f;

	float MipRate = MipTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;

	float MaxDist = 0;
	float ShadowStrength = 0;
	float HitCount = 0;
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];

		// give a little gap for preventing self-shadowing, along the vertex normal direction
		// distant pixel needs higher TMin
		float Gap = lerp(0.01f, 0.5f, saturate(MipRate * RT_MIPRATESCALE));

		RayDesc ShadowRay = (RayDesc)0;
		ShadowRay.Origin = WorldPos + WorldNormal * Gap;
		ShadowRay.Direction = -DirLight.Dir;

		ShadowRay.TMin = Gap;
		ShadowRay.TMax = float(1 << 20);

		UHDefaultPayload Payload = (UHDefaultPayload)0;
		TraceRay(TLAS, 0, 0xff, 0, 0, 0, ShadowRay, Payload);

		// store the max hit T to the result, system will perform PCSS later
		// also output shadow strength (Color.a)
		if (Payload.IsHit())
		{
			MaxDist = max(MaxDist, Payload.HitT);
			ShadowStrength += DirLight.Color.a;
			HitCount++;
		}
	}

	ShadowStrength *= 1.0f / max(HitCount, 1.0f);
	Result[PixelCoord] = float2(MaxDist, ShadowStrength);
}