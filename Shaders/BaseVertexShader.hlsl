#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

VertexOutput BaseVS(VertexInput Vin)
{
	VertexOutput Vout = (VertexOutput)0;
	float3 WorldPos = mul(float4(Vin.Position, 1.0f), UHWorld).xyz;

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj);
	Vout.UV0 = Vin.UV0;

	// transform normal by world IT
	Vout.Normal = LocalToWorldNormal(Vin.Normal);

#if WITH_NORMAL
	// calculate world TBN if normal map is used
	Vout.WorldTBN = CreateTBN(Vout.Normal, Vin.Tangent);
#endif

#if WITH_ENVCUBE
	Vout.WorldPos = WorldPos;
#endif

	return Vout;
}