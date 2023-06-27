#define UHDIRLIGHT_BIND t1
#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u3);
SamplerState LinearClampped : register(s4);
Texture2D RTShadow : register(t5);

// hard code to 5x5 for now, for different preset, set different define from C++ side
#define PCSS_INNERLOOP 2
#define PCSS_WEIGHT 0.04f
#define PCSS_MINPENUMBRA 1.0f
#define PCSS_MAXPENUMBRA 20.0f
#define PCSS_BLOCKERSCALE 0.01f
#define PCSS_DISTANCESCALE 0.01f

// group optimization
groupshared float GDepthCache[UHTHREAD_GROUP2D_X][UHTHREAD_GROUP2D_Y];

// simple PCSS sampling
float ShadowPCSS(Texture2D ShadowMap, Texture2D MipRateTex, float2 UV, uint2 GTid, float BaseDepth)
{
	// pre-sample the texture and cache it
	GDepthCache[GTid.x][GTid.y] = BaseDepth;
	GroupMemoryBarrierWithGroupSync();

	float DepthDiff = 0;

	// find max depth difference, fixed to 3x3
	UHUNROLL
	for (int I = -1; I <= 1; I++)
	{
		UHUNROLL
		for (int J = -1; J <= 1; J++)
		{
			float2 DepthPos = min(GTid.xy + float2(I, J), float2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
			float Depth = GDepthCache[DepthPos.x][DepthPos.y];
			DepthDiff = max(abs(Depth - BaseDepth), DepthDiff);
		}
	}

	// after getting distance to blocker, scale it down (or not) as penumbra number
	float DistToBlocker = RTShadow[UV * UHShadowResolution.xy].r;
	float Penumbra = lerp(PCSS_MINPENUMBRA, PCSS_MAXPENUMBRA, saturate(DistToBlocker * PCSS_BLOCKERSCALE));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
	float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;
	float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
	Penumbra = lerp(Penumbra, 0, MipWeight);

	// since this is screen space sampling, there is a chance to sample a lit pixel with a shadowed pixel
	// depth scaling reduces white artifacts when it's sampling neightbor pixels
	Penumbra = lerp(Penumbra, 0, saturate(DepthDiff / 0.001f));

	// actually sampling
	float Atten = 0.0f;
	UHUNROLL
	for (I = -PCSS_INNERLOOP; I <= PCSS_INNERLOOP; I++)
	{
		UHUNROLL
		for (int J = -PCSS_INNERLOOP; J <= PCSS_INNERLOOP; J++)
		{
			float2 Pos = UV * UHShadowResolution.xy + float2(I, J) * Penumbra;
			Pos = min(Pos, UHShadowResolution.xy - 1);
			float2 Shadow = RTShadow[Pos].rg;
			Atten += lerp(1.0f, Shadow.g, Shadow.r > 0);
		}
	}
	Atten *= PCSS_WEIGHT;

	return Atten;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	float Depth = SceneBuffers[3][DTid.xy].r;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
	if (Depth == 0.0f || UHNumDirLights == 0)
	{
		return;
	}

	float2 UV = float2(DTid.xy + 0.5f) * UHResolution.zw;

	// reconstruct world position
	float3 WorldPos = ComputeWorldPositionFromDeviceZ(DTid.xy, Depth);

	// get diffuse color, a is occlusion
	float4 Diffuse = SceneBuffers[0][DTid.xy];

	// unpack normal from [0,1] to [-1,1]
	float3 Normal = SceneBuffers[1][DTid.xy].xyz;
	Normal = Normal * 2.0f - 1.0f;

	// get specular color, a is roughness
	float4 Specular = SceneBuffers[2][DTid.xy];

	float3 Result = 0;

	// sample shadows
	float ShadowMask = 1.0f;
#if WITH_RTSHADOWS
	ShadowMask = ShadowPCSS(RTShadow, SceneBuffers[4], UV, GTid.xy, Depth);
#endif

	// dir lights accumulation
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		Result += LightBRDF(UHDirLights[Ldx], Diffuse.rgb, Specular, Normal, WorldPos, ShadowMask);
	}

	// indirect light accumulation
	Result += LightIndirect(Diffuse.rgb, Normal, Diffuse.a);

	SceneResult[DTid.xy] = float4(Result, 0);
}