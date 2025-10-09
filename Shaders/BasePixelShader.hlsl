#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHLightCommon.hlsli"
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
	//%UHS_INPUT
}

void BasePS(VertexOutput Vin
	, bool bIsFrontFace : SV_IsFrontFace
	, out float4 OutColor : SV_Target0
	, out float4 OutNormal : SV_Target1
	, out float4 OutMaterial : SV_Target2
	, out float4 OutEmissive : SV_Target3
	, out float OutMip : SV_Target4
	, out uint OutData : SV_Target5)
{
	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);

	UHBRANCH
    if (GBlendMode == UH_ISMASKED)
    {
        clip(MaterialInput.Opacity - GCutoff);
    }

	float3 BaseColor = MaterialInput.Diffuse;
	float Occlusion = MaterialInput.Occlusion;
	float Metallic = MaterialInput.Metallic;
	float Roughness = MaterialInput.Roughness;
    float Smoothness = 1.0f - Roughness;
    uint PackedData = 0;

	BaseColor = BaseColor - BaseColor * Metallic;
	OutColor = float4(saturate(BaseColor), Occlusion);

	// output normal in [0,1], a is unused at the moment, also be sure to flip normal based on face
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

	// output specular color and smoothness
	float3 Specular = MaterialInput.Specular;
	Specular = ComputeSpecularColor(Specular, MaterialInput.Diffuse, Metallic);
    OutMaterial = float4(Specular, Smoothness);

	// output emissive color and fresnel factor
    OutEmissive = float4(MaterialInput.Emissive.rgb, MaterialInput.FresnelFactor);

	// store the max change rate of UV for mip outside pixel shader
	float2 Dx = ddx_fine(Vin.UV0);
	float2 Dy = ddy_fine(Vin.UV0);
	float DeltaMax = max(length(Dx), length(Dy));
    OutMip = DeltaMax;
    OutData = PackedData;
	
	// store normal and setup the flag, 2-bit alpha channel can store 0.0f 1/3 2/3 1.0f
    OutNormal = float4(EncodeNormal(BumpNormal), UH_OPAQUE_MASK);
}