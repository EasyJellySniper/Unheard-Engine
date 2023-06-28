#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

ByteAddressBuffer OcclusionVisible : register(t3);
StructuredBuffer<float2> UV0Buffer : register(t4);
StructuredBuffer<float3> NormalBuffer : register(t5);
StructuredBuffer<float4> TangentBuffer : register(t6);

VertexOutput BaseVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	VertexOutput Vout = (VertexOutput)0;

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

	// calculate jitter
	float4x4 JitterMatrix = GetDistanceScaledJitterMatrix(length(WorldPos - UHCameraPos));

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj_NonJittered);
	Vout.Position = mul(Vout.Position, JitterMatrix);
	Vout.UV0 = UV0Buffer[Vid];

#if WITH_TANGENT_SPACE
	// calculate world TBN if normal map is used
	Vout.WorldTBN = CreateTBN(LocalToWorldNormal(NormalBuffer[Vid]), TangentBuffer[Vid]);
#else
	// transform normal by world IT
	Vout.Normal = LocalToWorldNormal(NormalBuffer[Vid]);
#endif

#if WITH_ENVCUBE
	Vout.WorldPos = WorldPos;
#endif

	return Vout;
}