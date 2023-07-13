// shader to soften ray tracing shadows
// based on PCSS algorithm
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

RWTexture2D<float2> RTShadow : register(u1);
RWTexture2D<float2> RTTranslucentShadow : register(u2);
Texture2D DepthTexture : register(t3);
Texture2D TranslucentTexture : register(t4);
Texture2D MipRateTex : register(t5);
SamplerState LinearClampped : register(s6);

// hard code to 5x5 for now, for different preset, set different define from C++ side in the future
#define PCSS_INNERLOOP 2
#define PCSS_WEIGHT 0.04f
#define PCSS_MINPENUMBRA 1.0f
#define PCSS_MAXPENUMBRA 20.0f
#define PCSS_BLOCKERSCALE 0.01f
#define PCSS_DISTANCESCALE 0.01f

// group optimization, use 1D array for the best perf
groupshared float GDepthCache[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

void SoftOpaqueShadow(uint2 PixelCoord, uint2 GTid, float2 UV, float MipRate)
{
	// pre-sample the texture and cache it
	float BaseDepth = DepthTexture.SampleLevel(LinearClampped, UV, 0).r;
	UHBRANCH
	if (BaseDepth == 0.0f)
	{
		return;
	}

	GDepthCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = BaseDepth;
	GroupMemoryBarrierWithGroupSync();

	float DepthDiff = 0;

	// find max depth difference, fixed to 3x3
	UHUNROLL
	for (int I = -1; I <= 1; I++)
	{
		UHUNROLL
		for (int J = -1; J <= 1; J++)
		{
			int2 DepthPos = min(int2(GTid.xy) + int2(I, J), int2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
			DepthPos = max(DepthPos, 0);

			float Depth = GDepthCache[DepthPos.x + DepthPos.y * UHTHREAD_GROUP2D_X];
			DepthDiff = max(abs(Depth - BaseDepth), DepthDiff);
		}
	}

	// after getting distance to blocker, scale it down (or not) as penumbra number
	float DistToBlocker = RTShadow[UV * UHShadowResolution.xy].r;
	float Penumbra = lerp(PCSS_MINPENUMBRA, PCSS_MAXPENUMBRA, saturate(DistToBlocker * PCSS_BLOCKERSCALE));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
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

	// ensure group is done reading g channel and store the result
	GroupMemoryBarrierWithGroupSync();
	RTShadow[PixelCoord * UHShadowResolution.xy / UHResolution.xy].g = Atten;
}

void SoftTranslucentShadow(uint2 PixelCoord, uint2 GTid, float2 UV, float MipRate)
{
	// pre-sample the texture and cache it
	float BaseDepth = TranslucentTexture.SampleLevel(LinearClampped, UV, 0).r;
	UHBRANCH
	if (BaseDepth == 0.0f)
	{
		return;
	}

	GDepthCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = BaseDepth;
	GroupMemoryBarrierWithGroupSync();

	float DepthDiff = 0;

	// find max depth difference, fixed to 3x3
	UHUNROLL
	for (int I = -1; I <= 1; I++)
	{
		UHUNROLL
		for (int J = -1; J <= 1; J++)
		{
			int2 DepthPos = min(int2(GTid.xy) + int2(I, J), int2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
			DepthPos = max(DepthPos, 0);

			float Depth = GDepthCache[DepthPos.x + DepthPos.y * UHTHREAD_GROUP2D_X];
			DepthDiff = max(abs(Depth - BaseDepth), DepthDiff);
		}
	}

	// after getting distance to blocker, scale it down (or not) as penumbra number
	float DistToBlocker = RTTranslucentShadow[UV * UHShadowResolution.xy].r;
	float Penumbra = lerp(PCSS_MINPENUMBRA, PCSS_MAXPENUMBRA, saturate(DistToBlocker * PCSS_BLOCKERSCALE));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
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
			float2 Shadow = RTTranslucentShadow[Pos].rg;
			Atten += lerp(1.0f, Shadow.g, Shadow.r > 0);
		}
	}
	Atten *= PCSS_WEIGHT;

	// ensure group is done reading g channel and store the result
	GroupMemoryBarrierWithGroupSync();
	RTTranslucentShadow[PixelCoord * UHShadowResolution.xy / UHResolution.xy].g = Atten;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void SoftRTShadowCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= UHResolution.x || DTid.y >= UHResolution.y)
	{
		return;
	}

	// early leave if there is no lights
	UHBRANCH
	if (UHNumDirLights == 0)
	{
		return;
	}

	float2 UV = float2(DTid.xy + 0.5f) * UHResolution.zw;
	float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;

	SoftOpaqueShadow(DTid.xy, GTid.xy, UV, MipRate);
	SoftTranslucentShadow(DTid.xy, GTid.xy, UV, MipRate);
}