#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

// compute version TAA
RWTexture2D<float4> SceneResult : register(u1);
Texture2D SceneTexture : register(t2);
Texture2D HistoryTexture : register(t3);
Texture2D MotionTexture : register(t4);
SamplerState PointSampler : register(s5);
SamplerState LinearSampler : register(s6);

static const float GHistoryWeightMin = 0.7f;
static const float GHistoryWeightMax = 0.9f;
static const float GMotionDiffScale = 50.0f;

// group optimization, use 1D array for the best perf
groupshared float3 GColorCache[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void TemporalAACS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
	{
		return;
	}

	float2 UV = float2(DTid.xy + 0.5f) * GResolution.zw;

	// sampling history is needed for ghosting
	float2 Motion = MotionTexture.SampleLevel(PointSampler, UV, 0).rg;
	float2 HistoryUV = UV - Motion;
	float MotionLength = length(Motion);

	// adjust the history weight based on the motion amount, make it less blurry when moving the camera
	float WeightBlend = 1.0f - saturate(MotionLength * GMotionDiffScale);
	float Weight = lerp(GHistoryWeightMin, GHistoryWeightMax, WeightBlend * WeightBlend);

	// if history UV is outside of the screen use the sample from current frame
    Weight = lerp(0, Weight, IsUVInsideScreen(HistoryUV));

	float3 Result = SceneTexture.SampleLevel(PointSampler, UV, 0).rgb;
	float3 HistoryResult = HistoryTexture.SampleLevel(LinearSampler, HistoryUV, 0).rgb;

	// cache the color sampling
	GColorCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = Result;
	GroupMemoryBarrierWithGroupSync();

	// 3x3 color clamping within group memory
	// this won't give the most precise color for the pixels that are close to the group edge
	// but it's fast and good enough for now
	float3 MaxColor = -UH_FLOAT_MAX;
    float3 MinColor = UH_FLOAT_MAX;

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
	HistoryResult = clamp(HistoryResult, MinColor, MaxColor);

	// final accumulation blend
	float3 Accumulation = lerp(Result, HistoryResult, Weight);
	SceneResult[DTid.xy] = float4(Accumulation, 1);
}