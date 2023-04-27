#include "../UHInputs.hlsli"

Texture2D SceneTexture : register(t1);
Texture2D HistoryTexture : register(t2);
Texture2D MotionTexture : register(t3);
Texture2D HistoryMotionTexture : register(t4);
Texture2D DepthTexture : register(t5);
SamplerState LinearSampler : register(s6);

static const float GHistoryWeightMin = 0.65f;
static const float GHistoryWeightMax = 0.8f;
static const float GMotionDiffScale = 500.0f;

float4 TemporalAAPS(PostProcessVertexOutput Vin) : SV_Target
{
	// sampling history is needed for ghosting
	float2 Motion = 0;
	float2 HistoryUV = 0;
	float2 HistoryMotion = 0;

	// 3x3 mask that find the max depth, and the max motion length vector for edges
	float Depth = 0;
	float MotionLength = 0;
	float HistoryMotionLength = 0;
	float ValidDepthCount = 0;

	UHUNROLL
	for (int Idx = -1; Idx <= 1; Idx++)
	{
		UHUNROLL
		for (int Jdx = -1; Jdx <= 1; Jdx++)
		{
			float2 TexUV = Vin.UV + float2(Idx, Jdx) * UHResolution.zw;
			float2 DepthPos = min(Vin.Position.xy + float2(Idx, Jdx), UHResolution.xy - 1);

			// max depth
			float CurrDepth = DepthTexture[DepthPos].r;
			Depth = max(CurrDepth, Depth);
			ValidDepthCount = lerp(ValidDepthCount, ValidDepthCount + 1, CurrDepth > 0);
		}
	}

	// treat depth count that is lower than the half of 3x3 as edge
	bool bIsEdge = ValidDepthCount < 4;
	UHBRANCH
	if (bIsEdge)
	{
		UHUNROLL
		for (Idx = -1; Idx <= 1; Idx++)
		{
			UHUNROLL
			for (int Jdx = -1; Jdx <= 1; Jdx++)
			{
				float2 TexUV = Vin.UV + float2(Idx, Jdx) * UHResolution.zw;

				// max length motion
				float2 CurrMotion = MotionTexture.SampleLevel(LinearSampler, TexUV, 0).rg;
				float2 CurrHistoryMotion = HistoryMotionTexture.SampleLevel(LinearSampler, TexUV - CurrMotion, 0).rg;

				float MoLength = length(CurrMotion);
				Motion = lerp(Motion, CurrMotion, MoLength > MotionLength);
				MotionLength = MoLength;

				MoLength = length(CurrHistoryMotion);
				HistoryMotion = lerp(HistoryMotion, CurrHistoryMotion, MoLength > HistoryMotionLength);
				HistoryMotionLength = MoLength;
			}
		}
		HistoryUV = Vin.UV - Motion;
	}
	else
	{
		Motion = MotionTexture.SampleLevel(LinearSampler, Vin.UV, 0).rg;
		HistoryUV = Vin.UV - Motion;
		HistoryMotion = HistoryMotionTexture.SampleLevel(LinearSampler, HistoryUV, 0).rg;
	}

	// adjust the history weight based on the motion amount, make it less blurry when moving the camera
	float WeightBlend = 1.0f - saturate(length(Motion) * GMotionDiffScale);
	float Weight = lerp(GHistoryWeightMin, GHistoryWeightMax, WeightBlend * WeightBlend);

	// solving disocclusion by comparing motion difference
	// also need to consider the case that an object suddenly changed its direction
	float MotionDiff = length(Motion - HistoryMotion);
	MotionDiff = saturate(MotionDiff * GMotionDiffScale);
	float MotionChanged = dot(Motion, HistoryMotion) < 0;

	// rejections
	Weight = lerp(Weight, 0, MotionDiff);
	Weight = lerp(Weight, 0, MotionChanged);
	Weight = lerp(0, Weight, Depth > 0);
	
	// if history UV is outside of the screen use the sample from current frame
	Weight = lerp(Weight, 0, HistoryUV.x != saturate(HistoryUV.x) || HistoryUV.y != saturate(HistoryUV.y));

	float3 Result = SceneTexture.SampleLevel(LinearSampler, Vin.UV, 0).rgb;
	float3 HistoryResult = HistoryTexture.SampleLevel(LinearSampler, HistoryUV, 0).rgb;
	float3 Accumulation = lerp(Result, HistoryResult, Weight);

	return float4(Accumulation, 1);
}