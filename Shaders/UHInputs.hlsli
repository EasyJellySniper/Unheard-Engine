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

#ifndef UHGBUFFER_BIND
#define UHGBUFFER_BIND t3
#endif

#define UHBRANCH [branch]
#define UHFLATTEN [flatten]
#define UHUNROLL [unroll]
#define UHLOOP [loop]

// thread group define for numthreads() use, always make it 64 now
#define UHTHREAD_GROUP1D 64
#define UHTHREAD_GROUP2D_X 8
#define UHTHREAD_GROUP2D_Y 8
#define UHTHREAD_GROUP3D_X 4
#define UHTHREAD_GROUP3D_Y 4
#define UHTHREAD_GROUP3D_Z 4

// system feature bits, used by GSystemRenderFeature in system constant
// this needs to sync with UHSystemRenderFeatureBits in C++ side
#define UH_ENV_CUBE 1 << 0
#define UH_HDR 1 << 1
#define UH_USE_SMOOTH_NORMAL_RAYTRACING 1 << 2
#define UH_DEBUGING 1 << 31

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
    uint GOpaqueSceneTextureIndex;
	// 1 = 3x3, 2 = 5x5..etc, will be clamped between 1 to 3
    int GPCSSKernal;
	
    float GFinalReflectionStrength;
    float GNearPlane;
    float GRTCullingDistance;
    float GPCSSMinPenumbra;
	
    float GPCSSMaxPenumbra;
    float GPCSSBlockerDistScale;
    uint GMaxReflectionRecursion;
    float GScreenMipCount;
}

// IT means inverse-transposed
cbuffer ObjectConstants : register(UHOBJ_BIND)
{
	float4x4 GWorld;
	float4x4 GWorldIT;
	float4x4 GPrevWorld;
	uint GInstanceIndex;
    float3 GWorldPos;
    float3 GBoundExtent;
}

// 0: Color + AO
// 1: Normal
// 2: Specular + Smoothness
// 3: Depth
Texture2D SceneBuffers[4] : register(UHGBUFFER_BIND);

#endif