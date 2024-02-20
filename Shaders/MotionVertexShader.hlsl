#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

StructuredBuffer<float2> UV0Buffer : register(t3);
StructuredBuffer<float3> NormalBuffer : register(t4);
StructuredBuffer<float4> TangentBuffer : register(t5);

MotionVertexOutput MotionObjectVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	MotionVertexOutput Vout = (MotionVertexOutput)0;

	float3 WorldPos = mul(float4(Position, 1.0f), UHWorld).xyz;
	float3 PrevWorldPos = mul(float4(Position, 1.0f), UHPrevWorld).xyz;

	// calculate jitter
	float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - UHCameraPos));

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj_NonJittered);
	Vout.CurrPos = Vout.Position;
	Vout.PrevPos = mul(float4(PrevWorldPos, 1.0f), UHPrevViewProj_NonJittered);

	Vout.Position = mul(Vout.Position, JitterMatrix);
	Vout.UV0 = UV0Buffer[Vid];
	Vout.Normal = LocalToWorldNormal(NormalBuffer[Vid]);
	
	// calculate world TBN if normal map is used
	UHBRANCH
    if (UHNeedWorldTBN)
    {
        Vout.WorldTBN = CreateTBN(Vout.Normal, TangentBuffer[Vid]);
    }

	return Vout;
}