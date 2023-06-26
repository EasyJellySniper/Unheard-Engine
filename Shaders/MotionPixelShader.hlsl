#include "../Shaders/UHInputs.hlsli"

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
}

float4 MotionObjectPS(MotionVertexOutput Vin) : SV_Target
{
#if WITH_OPACITY && !defined(WITH_DEPTHPREPASS)
	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);
	clip(MaterialInput.Opacity - GCutoff);
#endif

	// calc current/prev clip pos and return motion
	float4 PrevNDCPos = mul(float4(Vin.PrevWorldPos, 1.0f), UHPrevViewProj_NonJittered);
	PrevNDCPos /= PrevNDCPos.w;
	float2 PrevScreenPos = (PrevNDCPos.xy * 0.5f + 0.5f);

	float4 CurrNDCPos = mul(float4(Vin.WorldPos, 1.0f), UHViewProj_NonJittered);
	CurrNDCPos /= CurrNDCPos.w;
	float2 CurrScreenPos = (CurrNDCPos.xy * 0.5f + 0.5f);

	float2 Velocity = CurrScreenPos.xy - PrevScreenPos.xy;
	return float4(Velocity, 0, 1);
}