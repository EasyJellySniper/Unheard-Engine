#ifndef UHCOMMOM_H
#define UHCOMMOM_H

// UH common HLSL include, mainly for calculating functions
#define UH_FLOAT_EPSILON 1.175494351e-38F

float3 ComputeWorldPositionFromDeviceZ(float2 ScreenPos, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(ScreenPos, Depth, 1);
	NDCPos.xy *= UHResolution.zw;
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? UHViewProjInv_NonJittered : UHViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

// input as uv
float3 ComputeWorldPositionFromDeviceZ_UV(float2 UV, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(UV, Depth, 1);
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? UHViewProjInv_NonJittered : UHViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

float3 ComputeSpecularColor(float3 Specular, float3 BaseColor, float Metallic)
{
	// lerp specular based on metallic, use 5% dieletric on specular for now
	return lerp(Specular * 0.05f, BaseColor, Metallic.rrr);
}

float3 LocalToWorldNormal(float3 Normal)
{
	// mul(IT_M, norm) => mul(norm, I_M) => {dot(norm, I_M.col0), dot(norm, I_M.col1), dot(norm, I_M.col2)}
	return normalize(mul(Normal, (float3x3)UHWorldIT));
}

float3 LocalToWorldDir(float3 Dir)
{
	return normalize(mul(Dir, (float3x3)UHWorld));
}

float3x3 CreateTBN(float3 InWorldNormal, float4 InTangent)
{
	float3 Tangent = LocalToWorldDir(InTangent.xyz);
	float3 Binormal = cross(InWorldNormal, Tangent) * InTangent.w;

	float3x3 TBN = float3x3(Tangent, Binormal, InWorldNormal);
	return TBN;
}

float3 SchlickFresnel(float3 SpecColor, float LdotH)
{
	float CosIncidentAngle = LdotH;

	// pow5 F0 is used
	float F0 = 1.0f - CosIncidentAngle;
	float3 ReflectPercent = SpecColor + (1.0f - SpecColor) * (F0 * F0 * F0 * F0 * F0);

	return ReflectPercent;
}

// m^4 used for blinn phong
float BlinnPhong(float M, float NdotH)
{
	M *= M;
	M *= M;
	M = max(M, UH_FLOAT_EPSILON);
	M *= 256.0f;
	float N = (M + 64.0f) / 64.0f;

	return pow(NdotH, M) * N;
}

float3 LightBRDF(UHDirectionalLight DirLight, float3 Diffuse, float4 Specular, float3 Normal, float3 WorldPos, float ShadowMask)
{
	float3 LightColor = DirLight.Color.rgb * ShadowMask;
	float3 LightDir = -DirLight.Dir;
	float3 ViewDir = -normalize(WorldPos - UHCameraPos);

	// Diffuse = N dot L
	float NdotL = saturate(dot(Normal, LightDir));
	float3 LightDiffuse = LightColor * NdotL;

	// Specular = SchlickFresnel * BlinnPhong * N dot L * 0.25
	// safe normalize half dir
	float3 HalfDir = (ViewDir + LightDir) / (length(ViewDir + LightDir) + UH_FLOAT_EPSILON);
	float LdotH = saturate(dot(LightDir, HalfDir));
	float NdotH = saturate(dot(Normal, HalfDir));

	// inverse roughness to smoothness
	float Smoothness = 1.0f - Specular.a;
	float3 LightSpecular = LightColor * SchlickFresnel(Specular.rgb, LdotH) * BlinnPhong(Smoothness, NdotH) * NdotL;

	return Diffuse.rgb * LightDiffuse + LightSpecular;
}

float3 LightIndirect(float3 Diffuse, float3 Normal, float Occlusion)
{
	float Up = Normal.y;
	float3 IndirectDiffuse = UHAmbientGround + saturate(Up * UHAmbientSky);

	return Diffuse * IndirectDiffuse * Occlusion;
}

#endif