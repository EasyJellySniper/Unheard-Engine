#include "../UHInputs.hlsli"

// compute version TAA
RWTexture2D<float4> SceneResult : register(u1);
Texture2D SceneTexture : register(t2);
Texture2D HistoryTexture : register(t3);
Texture2D MotionTexture : register(t4);
Texture2D HistoryMotionTexture : register(t5);
Texture2D DepthTexture : register(t6);
SamplerState LinearSampler : register(s7);

static const float GHistoryWeightMin = 0.65f;
static const float GHistoryWeightMax = 0.8f;
static const float GMotionDiffScale = 500.0f;

// group optimization
groupshared float GDepthCache[UHTHREAD_GROUP2D_X][UHTHREAD_GROUP2D_Y];

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void TemporalAACS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= UHResolution.x || DTid.y >= UHResolution.y)
	{
		return;
	}

	float2 UV = float2(DTid.xy + 0.5f) * UHResolution.zw;

	// sampling history is needed for ghosting
	float2 Motion = 0;
	float2 HistoryUV = 0;
	float2 HistoryMotion = 0;

	Motion = MotionTexture.SampleLevel(LinearSampler, UV, 0).rg;
	HistoryUV = UV - Motion;
	HistoryMotion = HistoryMotionTexture.SampleLevel(LinearSampler, HistoryUV, 0).rg;

	// 3x3 mask that find the max depth, optimized with group shared memory
	GDepthCache[GTid.x][GTid.y] = DepthTexture[DTid.xy].r;
	GroupMemoryBarrierWithGroupSync();

	float Depth = 0;
	UHUNROLL
	for (int Idx = -1; Idx <= 1; Idx++)
	{
		UHUNROLL
		for (int Jdx = -1; Jdx <= 1; Jdx++)
		{
			float2 DepthPos = min(GTid.xy + float2(Idx, Jdx), float2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));

			// max depth
			float CurrDepth = GDepthCache[DepthPos.x][DepthPos.y];
			Depth = max(CurrDepth, Depth);
		}
	}

	// adjust the history weight based on the motion amount, make it less blurry when moving the camera
	float WeightBlend = 1.0f - saturate(length(Motion) * GMotionDiffScale);
	float Weight = lerp(GHistoryWeightMin, GHistoryWeightMax, WeightBlend * WeightBlend);

	// solving disocclusion by comparing motion difference and color clamping
	float MotionDiff = length(Motion - HistoryMotion);
	MotionDiff = saturate(MotionDiff * GMotionDiffScale);

	// motion rejection
	Weight = lerp(Weight, 0, saturate(MotionDiff / 0.1f));
	Weight = lerp(Weight, 0, Depth <= 0);

	// if history UV is outside of the screen use the sample from current frame
	Weight = lerp(Weight, 0, HistoryUV.x != saturate(HistoryUV.x) || HistoryUV.y != saturate(HistoryUV.y));

	// final accumulation blend
	float3 Result = SceneTexture.SampleLevel(LinearSampler, UV, 0).rgb;
	float3 HistoryResult = HistoryTexture.SampleLevel(LinearSampler, HistoryUV, 0).rgb;
	float3 Accumulation = lerp(Result, HistoryResult, Weight);
	SceneResult[DTid.xy] = float4(Accumulation, 1);
}