#include "../UHInputs.hlsli"

// compute version TAA
RWTexture2D<float4> SceneResult : register(u1);
Texture2D SceneTexture : register(t2);
Texture2D HistoryTexture : register(t3);
Texture2D MotionTexture : register(t4);
Texture2D HistoryMotionTexture : register(t5);
Texture2D DepthTexture : register(t6);
SamplerState LinearSampler : register(s7);

static const float GHistoryWeightMin = 0.7f;
static const float GHistoryWeightMax = 0.9f;
static const float GMotionDiffRawScale = 5.0f;
static const float GMotionDiffPixelScale = 0.5f;

// group optimization, use 1D array for the best perf
groupshared float3 GColorCache[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

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
	float MotionLength = 0;

	Motion = MotionTexture.SampleLevel(LinearSampler, UV, 0).rg;
	MotionLength = length(Motion);
	HistoryUV = UV - Motion;
	HistoryMotion = HistoryMotionTexture.SampleLevel(LinearSampler, HistoryUV, 0).rg;

	// adjust the history weight based on the motion amount, make it less blurry when moving the camera
	float WeightBlend = 1.0f - saturate(MotionLength * GMotionDiffRawScale);
	float Weight = lerp(GHistoryWeightMin, GHistoryWeightMax, WeightBlend * WeightBlend);

	// if history UV is outside of the screen use the sample from current frame
	Weight = lerp(Weight, 0, HistoryUV.x != saturate(HistoryUV.x) || HistoryUV.y != saturate(HistoryUV.y));

	float3 Result = SceneTexture.SampleLevel(LinearSampler, UV, 0).rgb;
	float3 HistoryResult = HistoryTexture.SampleLevel(LinearSampler, HistoryUV, 0).rgb;

	// cache the depth sampling
	GColorCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = Result;
	GroupMemoryBarrierWithGroupSync();

	// 3x3 color clamping
	float3 MaxColor = -999;
	float3 MinColor = 999;

	UHUNROLL
	for (int Idx = -1; Idx <= 1; Idx++)
	{
		UHUNROLL
		for (int Jdx = -1; Jdx <= 1; Jdx++)
		{
			int2 Pos = min(int2(GTid.xy) + int2(Idx, Jdx), int2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
			Pos = max(Pos, 0);

			float3 Color = GColorCache[Pos.x + Pos.y * UHTHREAD_GROUP2D_X];
			MinColor = min(MinColor, Color);
			MaxColor = max(MaxColor, Color);
        }
	}

	// adaptively select between color clamping and depth/motion rejection 
	UHBRANCH
	if (MotionLength > 0)
	{
		// clamp color for edge
		HistoryResult = clamp(HistoryResult, MinColor, MaxColor);
	}
	else
	{
		// depth rejection
		float BaseDepth = DepthTexture.SampleLevel(LinearSampler, UV, 0).r;
		Weight = lerp(Weight, 0, BaseDepth == 0.0f);

		float MotionDiff = length((Motion - HistoryMotion) * UHResolution.xy);
		Weight = lerp(Weight, 0, saturate(MotionDiff * GMotionDiffPixelScale));
	}

	// final accumulation blend
	float3 Accumulation = lerp(Result, HistoryResult, Weight);
	SceneResult[DTid.xy] = float4(Accumulation, 1);
}