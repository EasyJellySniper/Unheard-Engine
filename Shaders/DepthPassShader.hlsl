#include "UHInputs.hlsli"

ByteAddressBuffer OcclusionVisible : register(t3);
Texture2D OpacityTex : register(t4);
SamplerState OpacitySampler : register(s5);
StructuredBuffer<float2> UV0Buffer : register(t6);

// output uv for alpha test only
struct DepthVertexOutput
{
	float4 Position : SV_POSITION;
#if WITH_OPACITY
	float2 UV0 : TEXCOORD0;
#endif
};

DepthVertexOutput DepthVS(float3 Position : POSITION, uint Vid : SV_VertexID)
{
	DepthVertexOutput Vout = (DepthVertexOutput)0;

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

	// pass through the vertex data
	Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj);

#if WITH_OPACITY
	Vout.UV0 = UV0Buffer[Vid];
#endif

	return Vout;
}

void DepthPS(DepthVertexOutput Vin)
{
	// only alpha test would trigger this route
	// opaque object doesn't need a pixel shader
#if WITH_OPACITY
	UHMaterialConstants Material = UHMaterials[0];
	float Opacity = OpacityTex.Sample(OpacitySampler, Vin.UV0).r * Material.DiffuseColor.a;
	clip(Opacity - Material.Cutoff);
#endif
}