// Input header for UH shaders, mainly for input defines
#ifndef UHINPUT_H
#define UHINPUT_H

// predefines for bind parameter
// shader implement should override these
#ifndef UHSYSTEM_BIND
#define UHSYSTEM_BIND b0
#endif

#ifndef UHOBJ_BIND
#define UHOBJ_BIND b1
#endif

#ifndef UHMAT_BIND
#define UHMAT_BIND t2
#endif

#ifndef UHDIRLIGHT_BIND
#define UHDIRLIGHT_BIND t0
#endif

#ifndef UHGBUFFER_BIND
#define UHGBUFFER_BIND t1
#endif

#define UHBRANCH [branch]
#define UHFLATTEN [flatten]
#define UHUNROLL [unroll]

struct VertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
	float3 Normal : NORMAL;
#if WITH_NORMAL
	// output TBN if normal mapping enabled
	float3x3 WorldTBN : TEXCOORD2;
#endif

#if WITH_ENVCUBE
	float3 WorldPos : TEXCOORD5;
#endif
};

struct PostProcessVertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD0;
};

cbuffer SystemConstants : register(UHSYSTEM_BIND)
{
	float4x4 UHViewProj;
	float4x4 UHViewProjInv;
	float4x4 UHViewProj_NonJittered;
	float4x4 UHViewProjInv_NonJittered;
	float4x4 UHPrevViewProj_NonJittered;
	float4 UHResolution;		// xy for resolution, zw for 1/resolution
	float4 UHShadowResolution; // xy for resolution, zw for 1/resolution
	float3 UHCameraPos;
	uint UHNumDirLights;

	float3 UHAmbientSky;
	float JitterOffsetX;
	float3 UHAmbientGround;
	float JitterOffsetY;

	float3 UHCameraDir;
	uint UHNumRTInstances;
}

// IT means inverse-transposed
cbuffer ObjectConstants : register(UHOBJ_BIND)
{
	float4x4 UHWorld;
	float4x4 UHWorldIT;
	float4x4 UHPrevWorld;
	uint UHInstanceIndex;
}

// material inputs from graph system
struct UHMaterialInputs
{
	float3 Diffuse;
	float Occlusion;
	float3 Specular;
	float3 Normal;
	float Opacity;
	float Metallic;
	float Roughness;
	float FresnelFactor;
	float ReflectionFactor;
	float3 Emissive;
};

struct UHMaterialConstants
{
	float4 DiffuseColor;
	float3 EmissiveColor;
	float AmbientOcclusion;
	float3 SpecularColor;
	float Metallic;
	float Roughness;
	float BumpScale;
	float Cutoff;
	float EnvCubeMipMapCount;
	float FresnelFactor;
	int OpacityTexIndex;
	int OpacitySamplerIndex;
	float ReflectionFactor;
};
StructuredBuffer<UHMaterialConstants> UHMaterials : register(UHMAT_BIND);

struct UHDirectionalLight
{
	float4 Color;
	float3 Dir;
	float Padding;
};
StructuredBuffer<UHDirectionalLight> UHDirLights : register(UHDIRLIGHT_BIND);

// 0: Color + AO
// 1: Normal
// 2: MRS
// 3: Depth
// 4: vertex normal + mipmap level
Texture2D SceneBuffers[5] : register(UHGBUFFER_BIND);

#endif