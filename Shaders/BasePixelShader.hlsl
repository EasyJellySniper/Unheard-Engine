#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"

TextureCube EnvCube : register(t7);
SamplerState EnvSampler : register(s8);

//%UHS_TEXTUREDEFINE

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
	, out float OutMipRate : SV_Target4)
{
	UHMaterialConstants Material = UHMaterials[0];

	// unjitter the UV for improving blurry texture
	float2 Dx = ddx_fine(Vin.UV0);
	float2 Dy = ddy_fine(Vin.UV0);
	Vin.UV0 = Vin.UV0 - (Dx * JitterOffsetX + Dy * JitterOffsetY);

	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);

	// only clip objects without prepass
	// otherwise, the equal test will suffice
#if WITH_ALPHATEST && !defined(WITH_DEPTHPREPASS)
	clip(MaterialInput.Opacity - Material.Cutoff);
#endif

	float3 BaseColor = MaterialInput.Diffuse;
	float Occlusion = MaterialInput.Occlusion;
	float Metallic = MaterialInput.Metallic;
	float Roughness = MaterialInput.Roughness;

	BaseColor = BaseColor - BaseColor * Metallic;
	OutColor = float4(saturate(BaseColor), Occlusion);

	// output normal in [0,1], a is unused at the moment, also be sure to flip normal based on face
	float3 VertexNormal = normalize(Vin.Normal);
	VertexNormal *= (bIsFrontFace) ? 1 : -1;

#if WITH_TANGENT_SPACE
	float3 BumpNormal = MaterialInput.Normal;

	// tangent to world space
	BumpNormal = mul(BumpNormal, Vin.WorldTBN);
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#else
	float3 BumpNormal = VertexNormal;
#endif

	OutNormal = float4(BumpNormal * 0.5f + 0.5f, 0);

	// output specular color and roughness
	float3 Specular = MaterialInput.Specular;
	Specular = ComputeSpecularColor(Specular, MaterialInput.Diffuse, Metallic);
	OutMaterial = float4(Specular * MaterialInput.ReflectionFactor, Roughness);

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

	IndirectSpecular = EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * UHAmbientSky * SpecFade * SchlickFresnel(Specular, lerp(0, NdotV, MaterialInput.FresnelFactor));

	// since indirect spec will be added directly and can't be scaled with NdotL, expose material parameter to scale down it
	IndirectSpecular *= MaterialInput.ReflectionFactor;
#endif

	// output emissive color, a is not used at the moment
	OutEmissive = float4(MaterialInput.Emissive.rgb + IndirectSpecular, 0);

	// store the max change rate of UV for mip outside pixel shader
	float DeltaMax = max(length(Dx), length(Dy));
	OutMipRate = DeltaMax;
}