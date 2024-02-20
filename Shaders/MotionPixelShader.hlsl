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
	//%UHS_INPUT_OpacityNormalRough
}

void MotionObjectPS(MotionVertexOutput Vin
	, bool bIsFrontFace : SV_IsFrontFace
	, out float4 OutVelocity : SV_Target0
	, out float4 OutNormal : SV_Target1
	, out float4 OutBump : SV_Target2
	, out float OutRoughness : SV_Target3)
{
	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);
	UHBRANCH
    if (GBlendMode == UH_ISMASKED && !UHPrepassDepthEnabled)
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

	// output a few buffer for special purposes (mainly used in ray tracing)
	UHBRANCH
    if (GBlendMode > UH_ISMASKED)
    {
        float3 VertexNormal = normalize(Vin.Normal);
        VertexNormal *= (bIsFrontFace) ? 1 : -1;
		
        float3 BumpNormal = VertexNormal;
		UHBRANCH
        if (GIsTangentSpace)
        {
            BumpNormal = MaterialInput.Normal;

			// tangent to world space
            BumpNormal = mul(BumpNormal, Vin.WorldTBN);
            BumpNormal *= (bIsFrontFace) ? 1 : -1;
        }
		
		// shared with opaque vertex normal, mark alpha as 1 here for differentiate
        OutNormal = float4(EncodeNormal(VertexNormal), 1);
        OutBump = float4(EncodeNormal(BumpNormal), 1);
        OutRoughness = MaterialInput.Roughness;
    }
}