#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

// it's better to organize the opacity into diffuse's alpha channel
// for now just fits FBX's structure, this is art-driven
// now these also have no reflection system, so I define all registers here
// if texture is null, just don't write to descriptor and leave it as null, my shader keyword will prevent accessing null texture
// these can be optimized in the future since samplers can be shared on CPU side
Texture2D DiffuseTex : register(t3);
SamplerState DiffuseSampler : register(s4);

Texture2D OcclusionTex : register(t5);
SamplerState OcclusionSampler : register(s6);

Texture2D SpecularTex : register(t7);
SamplerState SpecularSampler : register(s8);

Texture2D NormalTex : register(t9);
SamplerState NormalSampler : register(s10);

Texture2D OpacityTex : register(t11);
SamplerState OpacitySampler : register(s12);

TextureCube EnvCube : register(t13);
SamplerState EnvSampler : register(s14);

Texture2D MetallicTex : register(t15);
SamplerState MetallicSampler : register(s16);

Texture2D RoughnessTex : register(t17);
SamplerState RoughnessSampler : register(s18);

void BasePS(VertexOutput Vin
	, bool bIsFrontFace : SV_IsFrontFace
	, out float4 OutColor : SV_Target0
	, out float4 OutNormal : SV_Target1
	, out float4 OutMaterial : SV_Target2
	, out float4 OutEmissive : SV_Target3
	, out float OutMipRate : SV_Target4)
{
	UHMaterialConstants Material = UHMaterials[0];

	// unjitter the UV for improving blurry texture
	Vin.UV0 = Vin.UV0 - (ddx_fine(Vin.UV0) * JitterOffsetX + ddy_fine(Vin.UV0) * JitterOffsetY);

	// if opacity is used, consider alpha test is needed
	// might want to separate this behavior in the future
#if WITH_OPACITY
	float Opacity = OpacityTex.Sample(OpacitySampler, Vin.UV0).r * Material.DiffuseColor.a;
	clip(Opacity - Material.Cutoff);
#endif

	// simply output LDR color to 1st buffer (consider metallic), A is used as ao here
#if WITH_DIFFUSE
	float3 BaseColor = DiffuseTex.Sample(DiffuseSampler, Vin.UV0).rgb * Material.DiffuseColor.rgb;
#else
	float3 BaseColor = Material.DiffuseColor.rgb;
#endif

#if WITH_OCCLUSION
	float Occlusion = OcclusionTex.Sample(OcclusionSampler, Vin.UV0).r * Material.AmbientOcclusion;
#else
	float Occlusion = Material.AmbientOcclusion;
#endif

#if WITH_METALLIC
	float Metallic = MetallicTex.Sample(MetallicSampler, Vin.UV0).r;
#else
	float Metallic = Material.Metallic;
#endif

#if WITH_ROUGHNESS
	float Roughness = RoughnessTex.Sample(RoughnessSampler, Vin.UV0).r * Material.Roughness;
#else
	float Roughness = Material.Roughness;
#endif

	BaseColor = BaseColor - BaseColor * Metallic;
	OutColor = float4(saturate(BaseColor), Occlusion);

	// output normal in [0,1], a is unused at the moment, also be sure to flip normal based on face
	float3 VertexNormal = normalize(Vin.Normal);
	VertexNormal *= (bIsFrontFace) ? 1 : -1;

#if WITH_NORMAL
	float3 BumpNormal = NormalTex.Sample(NormalSampler, Vin.UV0).xyz;
	BumpNormal = BumpNormal * 2.0f - 1.0f;

	// scale bump normal
	BumpNormal *= Material.BumpScale;

	// tangent to world space
	BumpNormal = normalize(mul(BumpNormal, Vin.WorldTBN));
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#else
	float3 BumpNormal = VertexNormal;
#endif

	OutNormal = float4(BumpNormal * 0.5f + 0.5f, 0);

	// output specular color and roughness
#if WITH_SPECULAR
	float3 Specular = SpecularTex.Sample(SpecularSampler, Vin.UV0).rgb * Material.SpecularColor;
#else
	float3 Specular = Material.SpecularColor;
#endif
	Specular = ComputeSpecularColor(Specular, Material.DiffuseColor.rgb, Metallic);
	OutMaterial = float4(Specular * Material.ReflectionFactor, Roughness);

	float3 IndirectSpecular = 0;
#if WITH_ENVCUBE
	// if per-object env cube is used, calculate it here, and adds the result to emissive buffer
	float3 EyeVector = normalize(Vin.WorldPos - UHCameraPos);
	float3 R = reflect(EyeVector, BumpNormal);
	float NdotV = abs(dot(BumpNormal, -EyeVector));

	float Smoothness = 1.0f - Roughness;
	float SpecFade = Smoothness * Smoothness;

	// use 1.0f - smooth * smooth as mip bias, so it will blurry with low smoothness
	// just don't want to declare another variable so simply use SpecFade
	float SpecMip = (1.0f - SpecFade) * Material.EnvCubeMipMapCount;

	IndirectSpecular = EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * UHAmbientSky * SpecFade * SchlickFresnel(Specular, lerp(0, NdotV, Material.FresnelFactor));

	// since indirect spec will be added directly and can't be scaled with NdotL, expose material parameter to scale down it
	IndirectSpecular *= Material.ReflectionFactor;
#endif

	// output emissive color, a is not used at the moment
	OutEmissive = float4(Material.EmissiveColor.rgb + IndirectSpecular, 0);

	// store the max change rate of UV for mip outside pixel shader
	float2 Dx = ddx_fine(Vin.UV0);
	float2 Dy = ddy_fine(Vin.UV0);
	float DeltaMax = max(length(Dx), length(Dy));
	OutMipRate = DeltaMax;
}