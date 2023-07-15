#define UHDIRLIGHT_BIND t4
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWTexture2D<float2> Result : register(u2);
RWTexture2D<float2> TranslucentResult : register(u3);
Texture2D MipTexture : register(t5);
Texture2D DepthTexture : register(t6);
Texture2D TranslucentDepthTexture : register(t7);
SamplerState PointSampler : register(s8);
SamplerState LinearSampler : register(s9);

void TraceOpaqueShadow(uint2 PixelCoord, float2 ScreenUV, float Depth, float MipRate, float MipLevel)
{
	UHBRANCH
	if (Depth == 0.0f)
	{
		// early return if no depth (no object)
		return;
	}

	// reconstruct world position and get world normal
	float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, Depth);

	// reconstruct normal, this needs to be done by a neighborhood pixel cross
	// to get more precise normal, either to shoot a "search" ray first or store vertex normal in another buffer
	// using bump normal for shadow is weird
	Depth = DepthTexture.SampleLevel(PointSampler, ScreenUV + float2(1, 0) * UHShadowResolution.zw, 0).r;
	float3 WorldPosRight = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, Depth);

	Depth = DepthTexture.SampleLevel(PointSampler, ScreenUV + float2(0, 1) * UHShadowResolution.zw, 0).r;
	float3 WorldPosDown = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, Depth);
	float3 WorldNormal = cross(WorldPosRight - WorldPos, WorldPosDown - WorldPos);

	float MaxDist = 0;
	float Atten = 1;

	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];

		// give a little gap for preventing self-shadowing, along the vertex normal direction
		// distant pixel needs higher TMin
		float Gap = lerp(0.1f, 0.5f, saturate(MipRate * RT_MIPRATESCALE));

		RayDesc ShadowRay = (RayDesc)0;
		ShadowRay.Origin = WorldPos + WorldNormal * Gap;
		ShadowRay.Direction = -DirLight.Dir;

		ShadowRay.TMin = Gap;
		ShadowRay.TMax = float(1 << 20);

		UHDefaultPayload Payload = (UHDefaultPayload)0;
		Payload.MipLevel = MipLevel;
		TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);

		// store the max hit T to the result, system will perform PCSS later
		// also output shadow atten (Color.a), with hit alpha blending
		if (Payload.IsHit())
		{
			MaxDist = max(MaxDist, Payload.HitT);
			Atten *= lerp(1.0f, DirLight.Color.a, Payload.HitAlpha);
		}
	}

	Result[PixelCoord] = float2(MaxDist, Atten);
}

void TraceTranslucentShadow(uint2 PixelCoord, float2 ScreenUV, float OpaqueDepth, float MipRate, float MipLevel)
{
	float TranslucentDepth = TranslucentDepthTexture.SampleLevel(PointSampler, ScreenUV, 0).r;

	UHBRANCH
	if (OpaqueDepth == TranslucentDepth)
	{
		// when opaque and translucent depth are equal, this pixel contains no translucent object, simply share the result from opaque and return
		TranslucentResult[PixelCoord] = Result[PixelCoord];
		return;
	}

	UHBRANCH
	if (TranslucentDepth == 0.0f)
	{
		// early return if there is no translucent at all
		return;
	}

	// reconstruct world position
	float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, TranslucentDepth);

	// reconstruct normal, this needs to be done by a neighborhood pixel cross
	// to get more precise normal, either to shoot a "search" ray first or store translucent's normal in another buffer
	TranslucentDepth = TranslucentDepthTexture.SampleLevel(PointSampler, ScreenUV + float2(1, 0) * UHShadowResolution.zw, 0).r;
	float3 WorldPosRight = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, TranslucentDepth);

	TranslucentDepth = TranslucentDepthTexture.SampleLevel(PointSampler, ScreenUV + float2(0, 1) * UHShadowResolution.zw, 0).r;
	float3 WorldPosDown = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, TranslucentDepth);
	float3 WorldNormal = cross(WorldPosRight - WorldPos, WorldPosDown - WorldPos);

	float MaxDist = 0;
	float Atten = 1;

	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];

		// give a little gap for preventing self-shadowing, along the vertex normal direction
		// distant pixel needs higher TMin, also needs a higher min value for translucent as normal vector is approximate
		float Gap = lerp(0.1f, 0.5f, saturate(MipRate * RT_MIPRATESCALE));

		RayDesc ShadowRay = (RayDesc)0;
		ShadowRay.Origin = WorldPos + WorldNormal * Gap;
		ShadowRay.Direction = -DirLight.Dir;

		ShadowRay.TMin = Gap;
		ShadowRay.TMax = float(1 << 20);

		UHDefaultPayload Payload = (UHDefaultPayload)0;
		Payload.MipLevel = MipLevel;
		TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, ShadowRay, Payload);

		// store the max hit T to the result, system will perform PCSS later
		// also output shadow atten (Color.a), with hit alpha blending
		if (Payload.IsHit())
		{
			MaxDist = max(MaxDist, Payload.HitT);
			Atten *= lerp(1.0f, DirLight.Color.a, Payload.HitAlpha);
		}
	}

	TranslucentResult[PixelCoord] = float2(MaxDist, Atten);
}

[shader("raygeneration")]
void RTShadowRayGen() 
{
	uint2 PixelCoord = DispatchRaysIndex().xy;
	Result[PixelCoord] = 0;
	TranslucentResult[PixelCoord] = 0;

	// early return if no lights
	UHBRANCH
	if (UHNumDirLights == 0)
	{
		return;
	}

	// to UV and get depth
	float2 ScreenUV = (PixelCoord + 0.5f) * UHShadowResolution.zw;
	float OpaqueDepth = DepthTexture.SampleLevel(PointSampler, ScreenUV, 0).r;

	// calculate mip level before ray tracing kicks off
	float MipRate = MipTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;
	float MipLevel = max(0.5f * log2(MipRate * MipRate), 0);

	// trace for translucent objs after opaque, the second OpaqueDepth isn't wrong since it will be compared in translucent function
	TraceOpaqueShadow(PixelCoord, ScreenUV, OpaqueDepth, MipRate, MipLevel);
	TraceTranslucentShadow(PixelCoord, ScreenUV, OpaqueDepth, MipRate, MipLevel);
}

[shader("miss")]
void RTShadowMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}