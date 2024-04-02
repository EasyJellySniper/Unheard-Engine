#include "UHInputs.hlsli"

struct SkyVertexOutput
{
	float4 Position : SV_POSITION;
	float3 LocalPos : TEXCOORD0;
};

SkyVertexOutput SkyboxVS(float3 Position : POSITION)
{
	SkyVertexOutput Vout = (SkyVertexOutput)0;
    float3 WorldPos = Position.xyz + GCameraPos.xyz;

	// doesn't need jitter VP for skybox
	Vout.Position = mul(float4(WorldPos, 1.0f), GViewProj_NonJittered);

	// always on the far plane (use 0 here since I use reversed-z)
	Vout.Position.z = 0;

	// set local pos for sampling texture cube later
	Vout.LocalPos = Position;

	return Vout;
}