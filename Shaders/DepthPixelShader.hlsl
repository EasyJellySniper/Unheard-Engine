#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMaterialCommon.hlsli"

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
	//%UHS_INPUT_OpacityOnly
	return (UHMaterialInputs)0;
}

void DepthPS(DepthVertexOutput Vin)
{
	// only alpha test will have this pixel shader, do cutoff anyway
#if MASKED
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);
	clip(MaterialInput.Opacity - GCutoff);
#endif
}