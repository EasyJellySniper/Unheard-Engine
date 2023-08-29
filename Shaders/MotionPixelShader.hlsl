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
}

void MotionObjectPS(MotionVertexOutput Vin
	, bool bIsFrontFace : SV_IsFrontFace
	, out float4 OutVelocity : SV_Target0
	, out float4 OutNormal : SV_Target1)
{
#if (WITH_ALPHATEST && !defined(WITH_DEPTHPREPASS)) || WITH_TRANSLUCENT
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

	OutVelocity = float4(CurrScreenPos.xy - PrevScreenPos.xy, 0, 1);

#if WITH_TRANSLUCENT
	float3 VertexNormal = normalize(Vin.Normal);
	VertexNormal *= (bIsFrontFace) ? 1 : -1;

	// a must be 1 to store normal as this is translucent pass
	OutNormal = float4(EncodeNormal(VertexNormal), 1);
#endif
}