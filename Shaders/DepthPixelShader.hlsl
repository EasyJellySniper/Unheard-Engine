#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"

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
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);

	clip(MaterialInput.Opacity - GCutoff);
#endif
}