#include "../Shaders/UHInputs.hlsli"

ByteAddressBuffer OcclusionVisible : register(t3);
StructuredBuffer<float2> UV0Buffer : register(t4);

// output uv for alpha test only
struct DepthVertexOutput
{
	float4 Position : SV_POSITION;
#if WITH_ALPHATEST
	float2 UV0 : TEXCOORD0;
#endif
};

//%UHS_TEXTUREDEFINE

UHMaterialInputs GetMaterialInput(float2 UV0)
{
	// material input code will be generated in C++ side
	//%UHS_INPUT
	return (UHMaterialInputs)0;
}

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

#if WITH_ALPHATEST
	Vout.UV0 = UV0Buffer[Vid];
#endif

	return Vout;
}

void DepthPS(DepthVertexOutput Vin)
{
	// only alpha test would trigger this route
	// opaque object doesn't need a pixel shader
#if WITH_ALPHATEST
	// unjitter the UV for improving blurry texture
	float2 Dx = ddx_fine(Vin.UV0);
	float2 Dy = ddy_fine(Vin.UV0);
	Vin.UV0 = Vin.UV0 - (Dx * JitterOffsetX + Dy * JitterOffsetY);

	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);

	UHMaterialConstants Material = UHMaterials[0];
	clip(MaterialInput.Opacity - Material.Cutoff);
#endif
}