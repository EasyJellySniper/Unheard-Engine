#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

struct MotionVertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
	float3 WorldPos : TEXCOORD1;
	float3 PrevWorldPos : TEXCOORD2;
};

Texture2D DepthTexture : register(t1);
SamplerState PointSampler : register(s2);

ByteAddressBuffer OcclusionVisible : register(t3);
Texture2D OpacityTex : register(t4);
SamplerState OpacitySampler : register(s5);
StructuredBuffer<float2> UV0Buffer : register(t6);

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
	float4 PrevNDCPos = mul(float4(WorldPos, 1.0f), UHPrevViewProj_NonJittered);
	PrevNDCPos /= PrevNDCPos.w;
	float2 PrevScreenPos = (PrevNDCPos.xy * 0.5f + 0.5f);

	float4 CurrNDCPos = mul(float4(WorldPos, 1.0f), UHViewProj_NonJittered);
	CurrNDCPos /= CurrNDCPos.w;
	float2 CurrScreenPos = (CurrNDCPos.xy * 0.5f + 0.5f);

	float2 Velocity = CurrScreenPos.xy - PrevScreenPos.xy;
	return float4(Velocity, 0, 1);
}

MotionVertexOutput MotionObjectVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	MotionVertexOutput Vout = (MotionVertexOutput)0;

#if WITH_OCCLUSION_TEST
	uint IsVisible = OcclusionVisible.Load(UHInstanceIndex * 4);
	UHBRANCH
	if (IsVisible == 0)
	{
		Vout.Position = -10;
		return Vout;
	}
#endif

	float3 WorldPos = mul(float4(Position, 1.0f), UHWorld).xyz;
	float3 PrevWorldPos = mul(float4(Position, 1.0f), UHPrevWorld).xyz;

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj);
	Vout.UV0 = UV0Buffer[Vid];
	Vout.WorldPos = WorldPos;
	Vout.PrevWorldPos = PrevWorldPos;

	return Vout;
}

float4 MotionObjectPS(MotionVertexOutput Vin) : SV_Target
{
#if WITH_OPACITY
	UHMaterialConstants Material = UHMaterials[0];
	float Opacity = OpacityTex.Sample(OpacitySampler, Vin.UV0).r * Material.DiffuseColor.a;
	clip(Opacity - Material.Cutoff);
#endif

	// calc current/prev clip pos and return motion
	float4 PrevNDCPos = mul(float4(Vin.PrevWorldPos, 1.0f), UHPrevViewProj_NonJittered);
	PrevNDCPos /= PrevNDCPos.w;
	float2 PrevScreenPos = (PrevNDCPos.xy * 0.5f + 0.5f);

	float4 CurrNDCPos = mul(float4(Vin.WorldPos, 1.0f), UHViewProj_NonJittered);
	CurrNDCPos /= CurrNDCPos.w;
	float2 CurrScreenPos = (CurrNDCPos.xy * 0.5f + 0.5f);

	float2 Velocity = CurrScreenPos.xy - PrevScreenPos.xy;
	return float4(Velocity, 0, 1);
}