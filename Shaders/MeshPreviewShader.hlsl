#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

struct VertexOut
{
	float4 Position : SV_POSITION;
};

cbuffer MeshPreviewConstant : register(b0)
{
	float4x4 ViewProj;
};

VertexOut MeshPreviewVS(float3 Position : POSITION)
{
	VertexOut Vout = (VertexOut)0;
	Vout.Position = mul(float4(Position, 1.0f), ViewProj);

	return Vout;
}

float4 MeshPreviewPS(VertexOut Vin) : SV_Target
{
	return float4(1,1,0,1);
}