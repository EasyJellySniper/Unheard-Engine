#include "../Shaders/UHInputs.hlsli"

ByteAddressBuffer OcclusionVisible : register(t3);
StructuredBuffer<float2> UV0Buffer : register(t4);

// texture/sampler tables for bindless rendering
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

cbuffer PassConstant : register(UHMAT_BIND)
{
	//%UHS_CBUFFERDEFINE
}

UHMaterialInputs GetMaterialInput(float2 UV0)
{
	// material input code will be generated in C++ side
	//%UHS_INPUT
	return (UHMaterialInputs)0;
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

	clip(MaterialInput.Opacity - GCutoff);
#endif
}