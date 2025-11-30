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
#if TRANSLUCENT
	, out float4 OutBump : SV_Target1
	, out float OutSmoothness : SV_Target2
	, out float OutMip : SV_Target3
	, out uint OutData : SV_Target4
#endif
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
    
	// output a few buffer for special purposes (mainly used in ray tracing)
#if TRANSLUCENT
	uint PackedData = 0;
    float3 VertexNormal = normalize(Vin.Normal);
    VertexNormal *= (bIsFrontFace) ? 1 : -1;
		
    float3 BumpNormal = VertexNormal;
#if TANGENT_SPACE
    BumpNormal = MaterialInput.Normal;

	// tangent to world space
    BumpNormal = mul(BumpNormal, Vin.WorldTBN);
    BumpNormal *= (bIsFrontFace) ? 1 : -1;
	PackedData |= UH_HAS_BUMP;
#endif
		
	// shared with opaque vertex normal, mark alpha as UH_TRANSLUCENT_MASK or UH_REFRACTION_MASK here for differentiate
	bool bRefraction = (GMaterialFeature & UH_REFRACTION);
	float Mask = bRefraction ? UH_REFRACTION_MASK : UH_TRANSLUCENT_MASK;
    OutBump = float4(EncodeNormal(BumpNormal), bRefraction);
	
    OutSmoothness = 1.0f - MaterialInput.Roughness;
		
    float2 Dx = ddx_fine(Vin.UV0);
    float2 Dy = ddy_fine(Vin.UV0);
    float DeltaMax = max(length(Dx), length(Dy));
    OutMip = DeltaMax;
	
	// data output
    OutData = PackedData;
#endif
}