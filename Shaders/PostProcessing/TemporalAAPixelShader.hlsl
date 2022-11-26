#include "../UHInputs.hlsli"

Texture2D SceneTexture : register(t1);
Texture2D HistoryTexture : register(t2);
Texture2D MotionTexture : register(t3);
Texture2D HistoryMotionTexture : register(t4);
Texture2D DepthTexture : register(t5);
SamplerState LinearSampler : register(s6);

static const float GHistoryWeight = 0.8f;

float4 TemporalAAPS(PostProcessVertexOutput Vin) : SV_Target
{
	float2 Motion = MotionTexture.SampleLevel(LinearSampler, Vin.UV, 0).rg;

	// sample history motion for solving disocclusion
	float2 HistoryUV = Vin.UV - Motion;
	float2 HistoryMotion = HistoryMotionTexture.SampleLevel(LinearSampler, HistoryUV, 0).rg;
	float Weight = GHistoryWeight;

	// solving disocclusion by comparing motion difference
	float MotionDiff = length(Motion - HistoryMotion);
	MotionDiff = saturate(MotionDiff * 1000.0f);
	Weight = lerp(Weight, 0, MotionDiff);

	// last puzzle, addressing pixels without depth
	// use a 3x3 max filter and check if it needs to sample history, better than set weight to 0 when there is no depth
	float Depth = 0;

	UHUNROLL
	for (int Idx = -1; Idx <= 1; Idx++)
	{
		UHUNROLL
		for (int Jdx = -1; Jdx <= 1; Jdx++)
		{
			float2 DepthPos = Vin.Position.xy + float2(Idx, Jdx);
			DepthPos = min(DepthPos, UHResolution.xy - 1);
			Depth = max(DepthTexture[DepthPos].r, Depth);
		}
	}
	Weight = lerp(0, Weight, Depth > 0);
	
	// if history UV is outside of the screen use the sample from current frame
	Weight = lerp(Weight, 0, HistoryUV.x != saturate(HistoryUV.x) || HistoryUV.y != saturate(HistoryUV.y));

	float3 Result = SceneTexture.SampleLevel(LinearSampler, Vin.UV, 0).rgb;
	float3 HistoryResult = HistoryTexture.SampleLevel(LinearSampler, HistoryUV, 0).rgb;
	float3 Accumulation = lerp(Result, HistoryResult, Weight);

	return float4(Accumulation, 1);
}