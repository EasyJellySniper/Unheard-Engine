#define UHDIRLIGHT_BIND t1
#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"

SamplerState PointClampped : register(s3);
SamplerState LinearClampped : register(s4);
Texture2D RTShadow : register(t5);

// hard code to 5x5 for now, for different preset, set different define from C++ side
#define PCSS_INNERLOOP 2
#define PCSS_WEIGHT 0.04f
#define PCSS_MINPENUMBRA 1.0f
#define PCSS_MAXPENUMBRA 20.0f
#define PCSS_BLOCKERSCALE 0.01f
#define PCSS_DISTANCESCALE 0.01f

float ShadowPCSS(Texture2D ShadowMap, Texture2D MipRateTex, float2 UV, float BaseDepth)
{
	float BlockCount = 0;
	float DistToBlocker = 0;
	float DepthDiff = 0;

	// simple PCSS, find distance to blocker first
	UHUNROLL
	for (int I = -PCSS_INNERLOOP; I <= PCSS_INNERLOOP; I++)
	{
		UHUNROLL
		for (int J = -PCSS_INNERLOOP; J <= PCSS_INNERLOOP; J++)
		{
			float ShadowData = RTShadow.SampleLevel(LinearClampped, UV + UHShadowResolution.zw * float2(I, J), 0).r;
			float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV + UHResolution.zw * float2(I, J), 0).r;
			DepthDiff += abs(Depth - BaseDepth);

			UHFLATTEN
			if (ShadowData > 0)
			{
				DistToBlocker += ShadowData;
				BlockCount++;
			}
		}
	}
	
	// after getting distance to blocker, scale it down (or not) as penumbra number
	DistToBlocker /= max(BlockCount, 1);
	float Penumbra = lerp(PCSS_MINPENUMBRA, PCSS_MAXPENUMBRA, saturate(DistToBlocker * PCSS_BLOCKERSCALE));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
	float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;
	float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
	Penumbra = lerp(Penumbra, 0, MipWeight);

	// since this is screen space sampling, there is a chance to sample a lit pixel with a shadowed pixel
	// depth scaling reduces white artifacts when it's sampling neightbor pixels
	Penumbra = lerp(Penumbra, 0, saturate(DepthDiff / 0.1f));

	// actually sampling
	float Atten = 0.0f;
	UHUNROLL
	for (I = -PCSS_INNERLOOP; I <= PCSS_INNERLOOP; I++)
	{
		UHUNROLL
		for (int J = -PCSS_INNERLOOP; J <= PCSS_INNERLOOP; J++)
		{
			float2 Shadow = RTShadow.SampleLevel(LinearClampped, UV + UHShadowResolution.zw * float2(I, J) * Penumbra, 0).rg;
			Atten += (Shadow.r > 0) * Shadow.g;
		}
	}
	Atten *= PCSS_WEIGHT;
	Atten = 1.0f - Atten;

	return Atten;
}

float4 LightPS(PostProcessVertexOutput Vin) : SV_Target
{
	float Depth = SceneBuffers[3].SampleLevel(PointClampped, Vin.UV, 0).r;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
	if (Depth == 0.0f || UHNumDirLights == 0)
	{
		return 0;
	}

	// reconstruct world position
	float3 WorldPos = ComputeWorldPositionFromDeviceZ(Vin.Position.xy, Depth);

	// get diffuse color, a is occlusion
	float4 Diffuse = SceneBuffers[0].SampleLevel(LinearClampped, Vin.UV, 0);

	// unpack normal from [0,1] to [-1,1]
	float3 Normal = SceneBuffers[1].SampleLevel(LinearClampped, Vin.UV, 0).xyz;
	Normal = Normal * 2.0f - 1.0f;

	// get specular color, a is roughness
	float4 Specular = SceneBuffers[2].SampleLevel(LinearClampped, Vin.UV, 0);

	float3 Result = 0;

	// sample shadows
	float ShadowMask = 1.0f;
#if WITH_RTSHADOWS
	ShadowMask = ShadowPCSS(RTShadow, SceneBuffers[4], Vin.UV, Depth);
#endif

	// dir lights accumulation
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		Result += LightBRDF(UHDirLights[Ldx], Diffuse.rgb, Specular, Normal, WorldPos, ShadowMask);
	}

	// indirect light accumulation
	Result += LightIndirect(Diffuse.rgb, Normal, Diffuse.a);

	return float4(Result, 0);
}