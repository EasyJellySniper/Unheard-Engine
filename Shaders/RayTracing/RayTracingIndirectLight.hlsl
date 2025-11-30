#define UHGBUFFER_BIND t4
#define UHDIRLIGHT_BIND t5
#define UHPOINTLIGHT_BIND t6
#define UHSPOTLIGHT_BIND t7
#define SH9_BIND t8
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"
#include "../UHSphericalHamonricCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWTexture2DArray<float4> OutResult : register(u2);

cbuffer RTIndirectConstants : register(b3)
{
    float GIndirectLightScale;
    float GIndirectLightFadeDistance;
    float GIndirectLightMaxTraceDist;
    float GIndirectOcclusionDistance;
    
    int GIndirectDownFactor;
    uint GIndirectRTSize;
}

Texture2D MixedMipTexture : register(t9);
Texture2D MixedDepthTexture : register(t10);
Texture2D<uint> MixedDataTexture : register(t11);
Texture2D SmoothSceneNormalTexture : register(t12);
Texture2D TranslucentBumpTexture : register(t13);
Texture2D TranslucentSmoothTexture : register(t14);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t15);
ByteAddressBuffer PointLightListTrans : register(t16);
ByteAddressBuffer SpotLightListTrans : register(t17);

static const int GMaxNumOfIndirectLightRays = 4;
// cherry-picked halton sequence, can be changed in the future
static const float2 GRayOffset[GMaxNumOfIndirectLightRays] =
{
    float2(0.5f, 0.3333f) * 2.0f - 1.0f,
    float2(0.25f, 0.6666f) * 2.0f - 1.0f,
    float2(0.125f, 0.4444f) * 2.0f - 1.0f,
    float2(0.375f, 0.2222f) * 2.0f - 1.0f
};

static const int GSmoothNormalDownFactor = 2;
static const int GMaxDirLight = 2;
static const float GIndirectLightMipBias = 12.0f;

int GetClosestAxis(float3 InVec)
{
    const float3 Axis[6] =
    {
        float3(1, 0, 0),
        float3(-1, 0, 0),
        float3(0, 1, 0),
        float3(0, -1, 0),
        float3(0, 0, 1),
        float3(0, 0, -1)
    };
    
    int ClosestAxis = 0;
    float MaxDot = 0.0f;
    
    UHUNROLL
    for (int Idx = 0; Idx < 6; Idx++)
    {
        float DotSum = dot(InVec, Axis[Idx]);
        if (DotSum > MaxDot)
        {
            MaxDot = DotSum;
            ClosestAxis = Idx;
        }
    }
    
    return ClosestAxis;
}

float TraceIndirectShadow(float3 HitWorldPos, uint TileIndex, in UHDefaultPayload Payload, float MipLevel)
{
    float Atten = 0.0f;
    float3 HitVertexNormal = Payload.HitVertexNormal;
    
    RayDesc ShadowRay = (RayDesc) 0;
    ShadowRay.Origin = HitWorldPos;
    ShadowRay.TMin = 0.01f;
    float Dummy = 0.0f;
    float Gap = 0.01f;
    
    // directional light shadows
    if (GNumDirLights > 0)
    {
        // trace directional light, 1 should be enough
        for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
        {
            UHDirectionalLight DirLight = UHDirLights[Ldx];
            if (TraceDiretionalShadow(TLAS, DirLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy))
            {
                break;
            }
        }
    }
    
    // point light shadows, reuse tile light information if hit pos is inside screen, otherwise fetch from instance data
    UHInstanceLights Lights = InstanceLights[Payload.HitInstanceIndex];
    
    uint PointTileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(PointTileOffset);
    if (Payload.IsInsideScreen && PointLightCount > 0)
    {
        PointTileOffset += 4;
        for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
        {
            uint PointLightIdx = PointLightListTrans.Load(PointTileOffset);
            PointTileOffset += 4;
            
            UHPointLight PointLight = UHPointLights[PointLightIdx];
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    else if (GNumPointLights > 0)
    {
        for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
        {
            uint LightIndex = Lights.PointLightIndices[Ldx];
            if (LightIndex == ~0)
            {
                break;
            }
            
            UHPointLight PointLight = UHPointLights[LightIndex];
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    
    // spot light shadow, reuse tile light information if hit pos is inside screen, otherwise fetch from instance data
    uint SpotTileOffset = GetSpotLightOffset(TileIndex);
    uint SpotLightCount = SpotLightListTrans.Load(SpotTileOffset);
    if (Payload.IsInsideScreen && SpotLightCount > 0)
    {
        SpotTileOffset += 4;
        for (uint Ldx = 0; Ldx < SpotLightCount; Ldx++)
        {
            uint LightIdx = SpotLightListTrans.Load(SpotTileOffset);
            SpotTileOffset += 4;
            
            UHSpotLight SpotLight = UHSpotLights[LightIdx];
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    else if (GNumSpotLights > 0)
    {
        for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
        {
            uint LightIndex = Lights.SpotLightIndices[Ldx];
            if (LightIndex == ~0)
            {
                break;
            }
            
            UHSpotLight SpotLight = UHSpotLights[LightIndex];
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    
    return saturate(Atten);
}

float4 CalculateIndirectLighting(in UHDefaultPayload Payload
    , float3 SceneNormal
    , float MipLevel)
{
    // doing the same lighting as object pass except the indirect specular
    float3 Result = 0;
    bool bHitTranslucent = (Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) > 0;
    float2 ScreenUV = bHitTranslucent ? Payload.HitScreenUVTrans : Payload.HitScreenUV;
    
    // blend the hit info with translucent opactiy when necessary
    UHDefaultPayload OriginPayload = Payload;
    if (bHitTranslucent)
    {
        float TransOpacity = Payload.HitEmissiveTrans.a;
        Payload.HitDiffuse = lerp(Payload.HitDiffuse, Payload.HitDiffuseTrans, TransOpacity);
        Payload.HitSpecular = lerp(Payload.HitSpecular, Payload.HitSpecularTrans, TransOpacity);
        Payload.HitVertexNormal = lerp(Payload.HitVertexNormal, Payload.HitVertexNormalTrans, TransOpacity);
        Payload.HitEmissive.rgb = lerp(Payload.HitEmissive.rgb, Payload.HitEmissiveTrans.rgb, TransOpacity);
    }
    Payload.HitVertexNormal = normalize(Payload.HitVertexNormal);
    
    // light calculation, be sure to normalize normal vector before using it
    // the specular part can also be ignored as that's not necessary for indirect diffuse
    uint2 PixelCoord = uint2(ScreenUV * GResolution.xy);
    uint TileX = CoordToTileX(PixelCoord.x);
    uint TileY = CoordToTileY(PixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    
    UHLightInfo IndirectLightInfo;
    IndirectLightInfo.Diffuse = Payload.HitDiffuse.rgb + Payload.HitEmissive.rgb;
    IndirectLightInfo.WorldPos = bHitTranslucent ? Payload.HitWorldPosTrans : Payload.HitWorldPos;
    IndirectLightInfo.AttenNoise = GetAttenuationNoise(PixelCoord.xy) * 0.1f;
    IndirectLightInfo.ShadowMask = TraceIndirectShadow(IndirectLightInfo.WorldPos, TileIndex, Payload, MipLevel);
    
    // consider indirect occlusion as well
    float IndirectOcclusion = saturate(Payload.HitT / GIndirectOcclusionDistance + IndirectLightInfo.AttenNoise);
    IndirectOcclusion = min(IndirectOcclusion, Payload.HitDiffuse.a);
    
    // calc indirect atten = distance atten x (1 - smoothness) x final scale
    // as high smoothness surfaces will reflect more than indirect diffuse
    float IndirectAtten = 1.0f - saturate(Payload.HitT / GIndirectLightFadeDistance + IndirectLightInfo.AttenNoise);
    IndirectAtten *= (1.0f - Payload.HitSpecular.a);
    IndirectAtten *= GIndirectLightScale;
    IndirectAtten *= IndirectOcclusion;
    
    float3 ObjColorTerm = IndirectLightInfo.Diffuse * IndirectLightInfo.ShadowMask * IndirectAtten;
    
    // dir indirect diffuse
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GMaxDirLight); Ldx++)
        {
            Result += UHDirLights[Ldx].Color.rgb * ObjColorTerm;
        }
    }
    
    // need to calculate attenuation for positional lights
    float PositionalLightAtten = 0.0f;
    
    // point indirect light
    if (GNumPointLights > 0)
    {
        uint TileOffset = GetPointLightOffset(TileIndex);
        uint PointLightCount = PointLightListTrans.Load(TileOffset);
        TileOffset += 4;
        
        // fetch tiled point light if the hit point is inside screen, otherwise need to lookup from instance lights
        UHBRANCH
        if (Payload.IsInsideScreen && PointLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
            {
                uint PointLightIdx = PointLightListTrans.Load(TileOffset);
                TileOffset += 4;
       
                UHPointLight PointLight = UHPointLights[PointLightIdx];
                PositionalLightAtten = CalculatePointLightAttenuation(PointLight.Radius, IndirectLightInfo.AttenNoise
                    , IndirectLightInfo.WorldPos - PointLight.Position);
                Result += PointLight.Color.rgb * ObjColorTerm * PositionalLightAtten;
            }
        }
        else
        {
            UHInstanceLights Lights = InstanceLights[Payload.HitInstanceIndex];
            for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
            {
                uint LightIndex = Lights.PointLightIndices[Ldx];
                if (LightIndex == ~0)
                {
                    break;
                }
            
                UHPointLight PointLight = UHPointLights[LightIndex];
                PositionalLightAtten = CalculatePointLightAttenuation(PointLight.Radius, IndirectLightInfo.AttenNoise
                    , IndirectLightInfo.WorldPos - PointLight.Position);
                Result += PointLight.Color.rgb * ObjColorTerm * PositionalLightAtten;
            }
        }
    }
    
    // spot indirect light
    if (GNumSpotLights > 0)
    {
        uint TileOffset = GetSpotLightOffset(TileIndex);
        uint SpotLightCount = SpotLightListTrans.Load(TileOffset);
        TileOffset += 4;
        
        UHBRANCH
        if (Payload.IsInsideScreen && SpotLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < SpotLightCount; Ldx++)
            {
                uint SpotLightIdx = SpotLightListTrans.Load(TileOffset);
                TileOffset += 4;
        
                UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
                PositionalLightAtten = CalculateSpotLightAttenuation(SpotLight.Radius, IndirectLightInfo.AttenNoise
                    , IndirectLightInfo.WorldPos - SpotLight.Position
                    , SpotLight.Dir, SpotLight.Angle, SpotLight.InnerAngle);
                Result += SpotLight.Color.rgb * ObjColorTerm * PositionalLightAtten;
            }
        }
        else
        {
            UHInstanceLights Lights = InstanceLights[Payload.HitInstanceIndex];
            for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
            {
                uint LightIndex = Lights.SpotLightIndices[Ldx];
                if (LightIndex == ~0)
                {
                    break;
                }

                UHSpotLight SpotLight = UHSpotLights[LightIndex];
                PositionalLightAtten = CalculateSpotLightAttenuation(SpotLight.Radius, IndirectLightInfo.AttenNoise
                    , IndirectLightInfo.WorldPos - SpotLight.Position
                    , SpotLight.Dir, SpotLight.Angle, SpotLight.InnerAngle);
                Result += SpotLight.Color.rgb * ObjColorTerm * PositionalLightAtten;
            }
        }
    }
    
    Result += ShadeSH9(IndirectLightInfo.Diffuse.rgb, float4(SceneNormal, 1.0f), IndirectAtten);
    
    return float4(Result, IndirectOcclusion);
}

[shader("raygeneration")]
void RTIndirectLightRayGen()
{
    uint FrameIndex = GFrameNumber % GMaxNumOfIndirectLightRays;
    uint2 PixelCoord = DispatchRaysIndex().xy * GIndirectDownFactor;
    uint3 OutputCoord = uint3(DispatchRaysIndex().xy, FrameIndex);
    
    // To UV
    float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;
    float4 OpaqueBump = SceneBuffers[1][PixelCoord];
    float4 TranslucentBump = TranslucentBumpTexture[PixelCoord];
    
    bool bHasOpaqueInfo = OpaqueBump.a == UH_OPAQUE_MASK;
    bool bHasTranslucentInfo = TranslucentBump.a == UH_TRANSLUCENT_MASK;
    bool bTraceThisPixel = bHasOpaqueInfo || bHasTranslucentInfo;
    if (!bTraceThisPixel)
    {
        return;
    }
    
    // Mip Level
    float MipRate = MixedMipTexture[PixelCoord].r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GIndirectLightMipBias;
    
    // World pos reconstruction
    float SceneDepth = MixedDepthTexture[PixelCoord].r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    
    // Scene normal
    uint PackedSceneData = MixedDataTexture[PixelCoord];
    bool bDenoise = (GSystemRenderFeature & UH_USE_SMOOTH_NORMAL_RAYTRACING);
    bool bHasBumpThisPixel = (PackedSceneData & UH_HAS_BUMP);
    
    float3 SceneNormal = 0;
    UHBRANCH
    if (bDenoise && bHasBumpThisPixel)
    {
        SceneNormal = SmoothSceneNormalTexture[PixelCoord / GSmoothNormalDownFactor].xyz;
    }
    else
    {
        SceneNormal = bHasTranslucentInfo ? TranslucentBump.xyz : OpaqueBump.xyz;
        SceneNormal = DecodeNormal(SceneNormal);
    }
    
    // Skip tracing from high smoothness surfaces, as the reflection has more influence than indirect diffuse for them
    float Smoothness = bHasTranslucentInfo ? TranslucentSmoothTexture[PixelCoord].r
        : SceneBuffers[2][PixelCoord].a;
    if (Smoothness > GRTReflectionSmoothCutoff)
    {
        return;
    }
    
    // Small ray rap applied for avoiding self-occlusion
    float RayGap = lerp(0.05f, 0.1f, saturate(MipRate * RT_MIPRATESCALE));
    
    // Ray setup and trace
    RayDesc IndirectRay = (RayDesc)0;
    IndirectRay.Origin = SceneWorldPos + SceneNormal * RayGap;
    {
        // Offset the scene normal as the ray direction
        // but need to figure out which two components to offset as the data is in world space
        int ClosestAxis = GetClosestAxis(SceneNormal);
        float3 RayDir = SceneNormal;
        
        if (ClosestAxis < 2)
        {
            // x is the forward axis, offset yz
            RayDir += float3(0, GRayOffset[FrameIndex]);
        }
        else if (ClosestAxis < 4)
        {
            // y is the forward axis, offset xz
            RayDir += float3(GRayOffset[FrameIndex].x, 0, GRayOffset[FrameIndex].y);
        }
        else
        {
            // z is the forward axis, offset xy
            RayDir += float3(GRayOffset[FrameIndex], 0);
        }
        
        RayDir = normalize(RayDir);
        IndirectRay.Direction = RayDir;
    }
    
    IndirectRay.TMin = RayGap;
    IndirectRay.TMax = GIndirectLightMaxTraceDist;

    UHDefaultPayload Payload = (UHDefaultPayload)0;
    Payload.MipLevel = MipLevel;
    Payload.PayloadData |= PAYLOAD_ISINDIRECTLIGHT;
    // init CurrentRecursion as 1
    Payload.CurrentRecursion = 1;
    
    // translucent object is considered too
    TraceRay(TLAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, IndirectRay, Payload);
    
    float4 Result = float4(0, 0, 0, 1);
    if (Payload.IsHit())
    {
        Result = CalculateIndirectLighting(Payload, SceneNormal, MipLevel);
        OutResult[OutputCoord] = Result;
    }
}

[shader("miss")]
void RTIndirectLightMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the result will just remain unchanged
}