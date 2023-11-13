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
#define UHMAT_BIND b2
#endif

#ifndef UHDIRLIGHT_BIND
#define UHDIRLIGHT_BIND t3
#endif

#ifndef UHGBUFFER_BIND
#define UHGBUFFER_BIND t4
#endif

#ifndef UHPOINTLIGHT_BIND
#define UHPOINTLIGHT_BIND t5
#endif

#ifndef UHSPOTLIGHT_BIND
#define UHSPOTLIGHT_BIND t6
#endif

#define UHBRANCH [branch]
#define UHFLATTEN [flatten]
#define UHUNROLL [unroll]
#define UHLOOP [loop]

#define UHTHREAD_GROUP2D_X 8
#define UHTHREAD_GROUP2D_Y 8
#define UHLIGHTCULLING_TILE 16
#define UHLIGHTCULLING_UPSCALE 2

struct VertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
#if WITH_TANGENT_SPACE
	// output TBN if normal mapping enabled
	float3x3 WorldTBN : TEXCOORD2;
#endif

	// will always output vertex normal
	float3 Normal : NORMAL;

#if defined(WITH_ENVCUBE) || defined(WITH_TRANSLUCENT)
	float3 WorldPos : TEXCOORD5;
#endif
};

struct DepthVertexOutput
{
	float4 Position : SV_POSITION;
#if WITH_ALPHATEST
	float2 UV0 : TEXCOORD0;
#endif
};

struct MotionVertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
	float3 WorldPos : TEXCOORD1;
	float3 PrevWorldPos : TEXCOORD2;
	float3 Normal : NORMAL;
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
    float4x4 UHProjInv;
    float4x4 UHProjInv_NonJittered;
    float4x4 UHView;
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

	float JitterScaleMin;	// minimum jitter scale
	float JitterScaleFactor;	// jitter scale factorto multiply with
    uint UHNumPointLights;
    uint UHLightTileCountX;
	
    uint UHMaxPointLightPerTile;
    uint UHNumSpotLights;
    uint UHMaxSpotLightPerTile;
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

struct UHDirectionalLight
{
	// color.a for intensity, but it's already applied to color
	float4 Color;
	float3 Dir;
    bool bIsEnabled;
};
StructuredBuffer<UHDirectionalLight> UHDirLights : register(UHDIRLIGHT_BIND);

struct UHPointLight
{
	// color.a for intensity, but it's already applied to color
    float4 Color;
    float Radius;
    float3 Position;
    bool bIsEnabled;
};
StructuredBuffer<UHPointLight> UHPointLights : register(UHPOINTLIGHT_BIND);

struct UHSpotLight
{
	// color.a for intensity, but it's already applied to color
    float4 Color;
    float3 Dir;
    float Radius;
    float Angle;
    float3 Position;
    float InnerAngle;
    bool bIsEnabled;
};
StructuredBuffer<UHSpotLight> UHSpotLights : register(UHSPOTLIGHT_BIND);

// 0: Color + AO
// 1: Normal
// 2: MRS
// 3: Depth
// 4: vertex normal + mipmap level
Texture2D SceneBuffers[5] : register(UHGBUFFER_BIND);

struct UHBoundingBox
{
    float3 BoxMax;
    float3 BoxMin;
};

#endif