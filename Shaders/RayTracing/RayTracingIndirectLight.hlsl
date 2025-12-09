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
    float GOcclusionEndDistance;
    
    float GOcclusionStartDistance;
    int GIndirectDownsampleFactor;
    bool GUseCache;
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
SamplerState LinearClampped : register(s18);

static const uint GMaxNumOfIndirectLightRays = 4;
// cherry-picked halton sequence, can be changed in the future
static const float2 GRayOffset[GMaxNumOfIndirectLightRays] =
{
    float2(0.5f, 0.3333f) * 2.0f - 1.0f,
    float2(0.25f, 0.6666f) * 2.0f - 1.0f,
    float2(0.125f, 0.4444f) * 2.0f - 1.0f,
    float2(0.375f, 0.2222f) * 2.0f - 1.0f
};

static const float GIndirectLightMipBias = 10.5f;

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
    , float2 ScreenUV
    , float3 ScenePos
    , float3 SceneNormal
    , float SceneSmoothness
    , float MipLevel
    , float RayGap)
{
    // doing the same lighting as object pass except the indirect specular
    float3 Result = 0;
    bool bHitTranslucent = (Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) > 0;
    float2 HitScreenUV = bHitTranslucent ? Payload.HitScreenUVTrans : Payload.HitScreenUV;
    
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
    uint2 HitPixelCoord = uint2(HitScreenUV * GResolution.xy);
    uint TileX = CoordToTileX(HitPixelCoord.x);
    uint TileY = CoordToTileY(HitPixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    
    UHLightInfo IndirectLightInfo;
    IndirectLightInfo.Diffuse = Payload.HitDiffuse.rgb + Payload.HitEmissive.rgb;
    IndirectLightInfo.WorldPos = bHitTranslucent ? Payload.HitWorldPosTrans : Payload.HitWorldPos;
    IndirectLightInfo.AttenNoise = GetAttenuationNoise(HitPixelCoord.xy) * 0.1f;
    IndirectLightInfo.ShadowMask = TraceIndirectShadow(IndirectLightInfo.WorldPos, TileIndex, Payload, MipLevel);
    
    // consider indirect occlusion for this pixel as well
    float AOThisPixel = saturate((Payload.HitT - GOcclusionStartDistance) / GOcclusionEndDistance + IndirectLightInfo.AttenNoise);
    // merge with the scene occlusion (output from object pass)
    AOThisPixel = min(AOThisPixel, SceneBuffers[0].SampleLevel(LinearClampped, ScreenUV, 0).a);
    
    // calc indirect atten = distance atten x (1 - smoothness) x final scale
    // as high smoothness surfaces will reflect more than indirect diffuse
    float IndirectAtten = 1.0f - saturate(Payload.HitT / GIndirectLightFadeDistance + IndirectLightInfo.AttenNoise);
    IndirectAtten *= (1.0f - Payload.HitSpecular.a) * (1.0f - SceneSmoothness);
    IndirectAtten *= GIndirectLightScale;
    IndirectAtten *= AOThisPixel;
    
    float3 ObjColorTerm = IndirectLightInfo.Diffuse * IndirectLightInfo.ShadowMask * IndirectAtten;
    
    // dir indirect diffuse
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
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
                    , ScenePos - PointLight.Position);
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
                    , ScenePos - PointLight.Position);
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
                    , ScenePos - SpotLight.Position
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
                    , ScenePos - SpotLight.Position
                    , SpotLight.Dir, SpotLight.Angle, SpotLight.InnerAngle);
                Result += SpotLight.Color.rgb * ObjColorTerm * PositionalLightAtten;
            }
        }
    }
    
    // indirect light from sky
    Result += ShadeSH9(IndirectLightInfo.Diffuse.rgb, float4(SceneNormal, 1.0f), IndirectAtten);
    
    return float4(Result, AOThisPixel);
}

[shader("raygeneration")]
void RTIndirectLightRayGen()
{
    // the system will now collect the tracing result with 4 samples
    // the tracings are distributed to 2 frames
    // E.g. 4 samples = 2 rays per frame
    uint OutputIndex = GFrameNumber % GNumOfIndirectFrames;
    uint3 OutputCoord = uint3(DispatchRaysIndex().xy, OutputIndex);
    OutResult[OutputCoord] = float4(0, 0, 0, 1);
    
    // at this point only pixel marked "o" will shoot the ray
    // which is 1/16 of the render resolution
    //   0 1 2 3 4 5 6 7
    // 0 o x x x o x x x
    // 1 x x x x x x x x
    // 2 x x x x x x x x
    // 3 x x x x x x x x
    // 4 o x x x o x x x
    // 5 x x x x x x x x
    // 6 x x x x x x x x
    // 7 x x x x x x x x
    
    uint2 PixelCoord = OutputCoord.xy * GIndirectDownsampleFactor;
    float2 ScreenUV = float2(PixelCoord) * GResolution.zw;
    
    float SceneDepth = MixedDepthTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    
    float4 OpaqueBump = SceneBuffers[1].SampleLevel(LinearClampped, ScreenUV, 0);
    float4 TranslucentBump = TranslucentBumpTexture.SampleLevel(LinearClampped, ScreenUV, 0);
    
    bool bHasOpaqueInfo = OpaqueBump.a > 0;
    bool bHasTranslucentInfo = TranslucentBump.a > 0;
    bool bTraceThisPixel = bHasOpaqueInfo || bHasTranslucentInfo;
    
    if (!bTraceThisPixel)
    {
        return;
    }
    
    float Smoothness = bHasTranslucentInfo ? TranslucentSmoothTexture[PixelCoord].r
        : SceneBuffers[2][PixelCoord].a;
    
    // Mip Level
    float MipRate = MixedMipTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GIndirectLightMipBias;
    
    // Scene normal
    uint PackedSceneData = MixedDataTexture[PixelCoord];
    bool bDenoise = (GSystemRenderFeature & UH_USE_SMOOTH_NORMAL_RAYTRACING);
    bool bHasBumpThisPixel = (PackedSceneData & UH_HAS_BUMP);
    
    float3 SceneNormal = 0;
    UHBRANCH
    if (bDenoise && bHasBumpThisPixel)
    {
        SceneNormal = SmoothSceneNormalTexture.SampleLevel(LinearClampped, ScreenUV, 0).xyz;
    }
    else
    {
        SceneNormal = bHasTranslucentInfo ? TranslucentBump.xyz : OpaqueBump.xyz;
        SceneNormal = DecodeNormal(SceneNormal);
    }
    
    // Small ray rap applied for avoiding self-occlusion
    float RayGap = lerp(0.05f, 0.1f, saturate(MipRate * RT_MIPRATESCALE));
    float4 Result = 0;
    
    UHUNROLL
    for (uint RayIdx = 0; RayIdx < GNumOfIndirectFrames; RayIdx++)
    {    
        // Even frames: Trace 0 1
        // Odd frames: Trace 2 3
        uint OffsetIndex = (GFrameNumber & 1) ? (RayIdx + 2) : RayIdx;
        
        // Ray setup and trace
        RayDesc IndirectRay = (RayDesc) 0;
        IndirectRay.Origin = SceneWorldPos + SceneNormal * RayGap;
        {
            // Offset the scene normal as the ray direction
            // but need to figure out which two components to offset as the data is in world space
            int ClosestAxis = GetClosestAxis(SceneNormal);
            float3 RayDir = SceneNormal;
        
            if (ClosestAxis < 2)
            {
            // x is the forward axis, offset yz
                RayDir += float3(0, GRayOffset[OffsetIndex]);
            }
            else if (ClosestAxis < 4)
            {
            // y is the forward axis, offset xz
                RayDir += float3(GRayOffset[OffsetIndex].x, 0, GRayOffset[OffsetIndex].y);
            }
            else
            {
            // z is the forward axis, offset xy
                RayDir += float3(GRayOffset[OffsetIndex], 0);
            }
        
            RayDir = normalize(RayDir);
            IndirectRay.Direction = RayDir;
        }
    
        IndirectRay.TMin = RayGap;
        IndirectRay.TMax = GIndirectLightMaxTraceDist;
        
        // see if it can use cached HitT for skipping trace this frame
        uint3 CacheOutputCoord = OutputCoord;
        CacheOutputCoord.z = OffsetIndex;

        UHDefaultPayload Payload = (UHDefaultPayload)0;
        Payload.MipLevel = MipLevel;
        Payload.PayloadData |= PAYLOAD_ISINDIRECTLIGHT;
        // init CurrentRecursion as 1
        Payload.CurrentRecursion = 1;
    
        // trace ray!
        TraceRay(TLAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, IndirectRay, Payload);
        
        if (Payload.IsHit())
        {
            Result += CalculateIndirectLighting(Payload, ScreenUV, SceneWorldPos, SceneNormal
                , Smoothness, MipLevel, RayGap);
        }
        else
        {
            Result += float4(0, 0, 0, 1);
        }
    }
    
    // avg the result and output
    OutResult[OutputCoord] = Result * 0.5f;
}

[shader("miss")]
void RTIndirectLightMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the result will just remain unchanged
}