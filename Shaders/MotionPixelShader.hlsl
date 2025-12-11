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
	//%UHS_INPUT_OpacityNormalRough
}

void MotionObjectPS(MotionVertexOutput Vin
	, bool bIsFrontFace : SV_IsFrontFace
	, out float4 OutVelocity : SV_Target0
)
{
	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);
	UHBRANCH
    if (GBlendMode == UH_ISMASKED)
    {
        clip(MaterialInput.Opacity - GCutoff);
    }
	
	UHBRANCH
    if (GBlendMode > UH_ISMASKED)
    {
        clip(MaterialInput.Opacity - 0.001f);
    }

	// calc current/prev clip pos and return motion
	Vin.PrevPos /= Vin.PrevPos.w;
	float2 PrevScreenPos = (Vin.PrevPos.xy * 0.5f + 0.5f);

	Vin.CurrPos /= Vin.CurrPos.w;
	float2 CurrScreenPos = (Vin.CurrPos.xy * 0.5f + 0.5f);

	OutVelocity = float4(CurrScreenPos.xy - PrevScreenPos.xy, 0, 1);
}