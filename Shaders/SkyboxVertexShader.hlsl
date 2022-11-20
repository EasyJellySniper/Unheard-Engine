#include "UHInputs.hlsli"

struct SkyVertexOutput
{
	float4 Position : SV_POSITION;
	float3 LocalPos : TEXCOORD0;
};

SkyVertexOutput SkyboxVS(VertexInput Vin)
{
	SkyVertexOutput Vout = (SkyVertexOutput)0;
	float3 WorldPos = mul(float4(Vin.Position, 1.0f), UHWorld).xyz;
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj);

	// always on the far plane (use 0 here since I use reversed-z)
	Vout.Position.z = 0;

	// set local pos for sampling texture cube later
	Vout.LocalPos = Vin.Position;

	return Vout;
}