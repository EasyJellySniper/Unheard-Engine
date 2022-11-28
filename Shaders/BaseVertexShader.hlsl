#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

StructuredBuffer<float2> UV0Buffer : register(t19);
StructuredBuffer<float3> NormalBuffer : register(t20);
StructuredBuffer<float4> TangentBuffer : register(t21);

VertexOutput BaseVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	VertexOutput Vout = (VertexOutput)0;
	float3 WorldPos = mul(float4(Position, 1.0f), UHWorld).xyz;

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj);
	Vout.UV0 = UV0Buffer[Vid];

	// transform normal by world IT
	Vout.Normal = LocalToWorldNormal(NormalBuffer[Vid]);

#if WITH_NORMAL
	// calculate world TBN if normal map is used
	Vout.WorldTBN = CreateTBN(Vout.Normal, TangentBuffer[Vid]);
#endif

#if WITH_ENVCUBE
	Vout.WorldPos = WorldPos;
#endif

	return Vout;
}