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

// system feature bits
#define UH_DEPTH_PREPASS 1 << 0
#define UH_ENV_CUBE 1 << 1
#define UH_HDR 1 << 2
#define UH_OCCLUSION_CULLING 1 << 3

struct VertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
	
#if TANGENT_SPACE
	// output TBN if normal mapping enabled
	float3x3 WorldTBN : TEXCOORD2;
#endif

	// will always output vertex normal
	float3 Normal : NORMAL;
	
#if TRANSLUCENT
	float3 WorldPos : TEXCOORD5;
#endif
};

struct DepthVertexOutput
{
	float4 Position : SV_POSITION;
#if MASKED
	float2 UV0 : TEXCOORD0;
#endif
};

struct MotionVertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV0 : TEXCOORD0;
	float4 CurrPos : TEXCOORD1;
	float4 PrevPos : TEXCOORD2;
	
#if TANGENT_SPACE && TRANSLUCENT
	// output TBN if normal mapping enabled
    float3x3 WorldTBN : TEXCOORD3;
#endif
	
#if TRANSLUCENT
	float3 Normal : NORMAL;
#endif
};

struct PostProcessVertexOutput
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD0;
};

cbuffer SystemConstants : register(UHSYSTEM_BIND)
{
	float4x4 GViewProj;
	float4x4 GViewProjInv;
	float4x4 GViewProj_NonJittered;
	float4x4 GViewProjInv_NonJittered;
	float4x4 GPrevViewProj_NonJittered;
    float4x4 GProjInv;
    float4x4 GProjInv_NonJittered;
    float4x4 GView;
	float4 GResolution;		// xy for resolution, zw for 1/resolution
	float4 GShadowResolution; // xy for resolution, zw for 1/resolution
	float3 GCameraPos;
	uint GNumDirLights;

	float3 GAmbientSky;
	float GJitterOffsetX;
	float3 GAmbientGround;
	float GJitterOffsetY;

	float3 GCameraDir;
	uint GNumRTInstances;

	float GJitterScaleMin;	// minimum jitter scale
	float GJitterScaleFactor;	// jitter scale factorto multiply with
    uint GNumPointLights;
    uint GLightTileCountX;
	
    uint GMaxPointLightPerTile;
    uint GNumSpotLights;
    uint GMaxSpotLightPerTile;
	uint GFrameNumber;
	
    uint GSystemRenderFeature;
	float GDirectionalShadowRayTMax;
	uint GLinearClampSamplerIndex;
	uint GSkyCubeSamplerIndex;
	
    uint GPointClampSamplerIndex;
    uint GRTReflectionQuality;
    float GRTReflectionRayTMax;
    float GRTReflectionSmoothCutoff;
	
    float GEnvCubeMipMapCount;
    uint GDefaultAnisoSamplerIndex;
    uint GRefractionClearIndex;
    uint GRefractionBlurIndex;
	
    float GFinalReflectionStrength;
    float GMotionVectorScale;
}

// IT means inverse-transposed
cbuffer ObjectConstants : register(UHOBJ_BIND)
{
	float4x4 GWorld;
	float4x4 GWorldIT;
	float4x4 GPrevWorld;
	uint GInstanceIndex;
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
	float Refraction;
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
// 2: Specular + Smoothness
// 3: Depth
Texture2D SceneBuffers[4] : register(UHGBUFFER_BIND);

struct UHBoundingBox
{
    float3 BoxMax;
    float3 BoxMin;
};

struct UHRendererInstance
{
    // mesh index to lookup mesh data
    uint MeshIndex;
    // indice type
    uint IndiceType;
};

#endif