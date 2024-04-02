#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

Texture2D DepthTexture : register(t1);
SamplerState PointSampler : register(s2);

float4 CameraMotionPS(PostProcessVertexOutput Vin) : SV_Target
{
	// this one simply builds motion vector from depth information
	// suitable for opaque objects
	// note that matrix used in this pass should be non-jittered!
	float Depth = DepthTexture.SampleLevel(PointSampler, Vin.UV, 0).r;

	// don't calculate in empty pixels
	UHBRANCH
	if (Depth == 0.0f)
	{
		return 0;
	}

	float3 WorldPos = ComputeWorldPositionFromDeviceZ(Vin.Position.xy, Depth, true);

	// calc current/prev clip pos
	float4 PrevNDCPos = mul(float4(WorldPos, 1.0f), GPrevViewProj_NonJittered);
	PrevNDCPos /= PrevNDCPos.w;
	float2 PrevScreenPos = (PrevNDCPos.xy * 0.5f + 0.5f);

	float4 CurrNDCPos = mul(float4(WorldPos, 1.0f), GViewProj_NonJittered);
	CurrNDCPos /= CurrNDCPos.w;
	float2 CurrScreenPos = (CurrNDCPos.xy * 0.5f + 0.5f);

	float2 Velocity = CurrScreenPos.xy - PrevScreenPos.xy;
	return float4(Velocity, 0, 1);
}