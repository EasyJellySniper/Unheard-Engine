#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

StructuredBuffer<float2> UV0Buffer : register(t3);
StructuredBuffer<float3> NormalBuffer : register(t4);
StructuredBuffer<float4> TangentBuffer : register(t5);

VertexOutput BaseVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	VertexOutput Vout = (VertexOutput)0;

	float3 WorldPos = mul(float4(Position, 1.0f), UHWorld).xyz;

	// calculate jitter
	float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - UHCameraPos));

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj_NonJittered);
	Vout.Position = mul(Vout.Position, JitterMatrix);
	Vout.UV0 = UV0Buffer[Vid];

	// calculate world TBN if normal map is used
	UHBRANCH
	if (UHNeedWorldTBN)
    {
        Vout.WorldTBN = CreateTBN(LocalToWorldNormal(NormalBuffer[Vid]), TangentBuffer[Vid]);
    }

	// transform normal by world IT
	Vout.Normal = LocalToWorldNormal(NormalBuffer[Vid]);
	Vout.WorldPos = WorldPos;

	return Vout;
}