#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"

StructuredBuffer<float2> UV0Buffer : register(t3);

DepthVertexOutput DepthVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	DepthVertexOutput Vout = (DepthVertexOutput)0;

	float3 WorldPos = mul(float4(Position, 1.0f), GWorld).xyz;

	// calculate jitter
	float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - GCameraPos));

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), GViewProj_NonJittered);
	Vout.Position = mul(Vout.Position, JitterMatrix);
#if MASKED
	Vout.UV0 = UV0Buffer[Vid];
#endif

	return Vout;
}