// RayTracingIndirectLight.hlsl - Shader for opaque indirect light tracing
#define UHGBUFFER_BIND t5
#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#define UHSPOTLIGHT_BIND t8
#define SH9_BIND t9
#include "../UHInputs.hlsli"
#include "../UHSphericalHamonricCommon.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWTexture2D<float4> OutDiffuse : register(u2);
RWTexture2D<float2> OutSkyData : register(u3);

cbuffer RTIndirectConstants : register(b4)
{
    float GIndirectLightScale;
    float GIndirectLightFadeDistance;
    float GIndirectLightMaxTraceDist;
    float GMinSkyConeAngle;
    
    float GMaxSkyConeAngle;
    int GIndirectDownsampleFactor;
    float GIndirectRayAngle;
}

Texture2D SceneMipTexture : register(t10);
Texture2D<uint> SceneDataTexture : register(t11);
Texture2D SmoothSceneNormalTexture : register(t12);
Texture3D SkyDataVoxel : register(t13);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t14);
ByteAddressBuffer PointLightList : register(t15);
ByteAddressBuffer SpotLightList : register(t16);

SamplerState PointClampped : register(s17);
SamplerState LinearClampped : register(s18);
SamplerState Point3DClampped : register(s19);
SamplerState Linear3DClampped : register(s20);

static const float GILMipBias = 7.5f;

// more cherry-picked sequences for indirect tracing use
// this is a mixture of Halton sequence and first few for the center-biased UV
static const float2 GRayOffset[8] =
{
    float2(0.5, 1.0),
    float2(0.9, 0.98),
    float2(0.98, 0.87),
    float2(0.25, 0.6667),
    float2(0.75, 0.1111),
    float2(0.125, 0.4444),
    float2(0.625, 0.7778),
    float2(0.375, 0.2222),
};

static const float GMaxSkyNeighborWeight = 8.0f;

uint GetJitterPhase2D(uint2 PixelCoord)
{
    uint PixelIndex1D = PixelCoord.x + PixelCoord.y * (GResolution.x / GIndirectDownsampleFactor);
    return (GFrameNumber * 3 + PixelIndex1D) % 8;
}

float4 SampleSkyData(float3 WorldPos, float3 Normal, uint2 PixelCoord, bool bCenterVoxelOnly)
{
    float3 SceneBoundMin = GSceneCenter - GSceneExtent;
    float3 BaseVolumeUV = (WorldPos - SceneBoundMin) / (2.0f * GSceneExtent);
    float3 VoxelSize = 1.0f / (2.0f * GSceneExtent);
    
    uint Phase = (GFrameNumber + PixelCoord.x * 13 + PixelCoord.y * 17) % 8;
    float3 Jitter = CoordinateToHash3(PixelCoord, Phase);
    BaseVolumeUV += Jitter * VoxelSize;
    
    int3 Offsets[27] =
    {
        int3(0, 0, 0),
        int3(1, 0, 0),
        int3(-1, 0, 0),
        int3(0, 1, 0),
        int3(0, -1, 0),
        int3(0, 0, 1),
        int3(0, 0, -1),
        int3(-1, -1, 0),
        int3(1, 1, 0),
        int3(1, 0, 1),
        int3(1, 0, -1),
        int3(1, -1, 0),
        int3(0, 1, 1),
        int3(0, 1, -1),
        int3(0, -1, 1),
        int3(0, -1, -1),
        int3(-1, 1, 0),
        int3(-1, 0, 1),
        int3(-1, 0, -1),
        int3(-1, 1, 1),
        int3(1, -1, -1),
        int3(-1, 1, -1),
        int3(1, -1, 1),
        int3(-1, -1, 1),
        int3(1, 1, -1),
        int3(-1, -1, -1),
        int3(1, 1, 1),
    };
    
    float3 GatherDir = 0;
    float GatherWeight = 0;
    
    // ----------------------- 27 tap nearest gathering to preserve sky direction
    UHUNROLL
    for (uint I = 0; I < 27; I++)
    {
        float3 VolumeUV = BaseVolumeUV + Offsets[I] * VoxelSize;
        float4 SkyData = SkyDataVoxel.SampleLevel(Point3DClampped, VolumeUV, 0);
        
        UHBRANCH
        if (bCenterVoxelOnly)
        {
            return SkyData;
        }
        
        // bias neighbors by surface normal, but allows slight back-facing contributions
        // helps around doors, thin walls, stair lips
        float NdotL = dot(Normal, SkyData.xyz);
        float NWeight = saturate((NdotL + 0.1f) * 1.1f);
        float CenterBias = (I == 0) ? 1.5f : 1.0f;
        float DistWeight = 1.0f / (1.0f + length(Offsets[I]));

        float W = SkyData.w * NWeight * CenterBias * DistWeight;
        GatherDir += SkyData.xyz * W;
        GatherWeight += W;
    }
    
    if (GatherWeight > 0 && dot(GatherDir, GatherDir) > UH_FLOAT_EPSILON)
    {
        // safe normalization
        GatherDir = normalize(GatherDir);
    }
    else
    {
        // fallback
        GatherDir = Normal;
    }
    
    GatherWeight = saturate(GatherWeight / GMaxSkyNeighborWeight);
    
    return float4(GatherDir, GatherWeight);
}

float2 TraceSkyVisibility(uint2 OutputCoord, float3 SceneWorldPos, float3 SceneNormal, float2 ScreenUV, float RayGap)
{
    if ((GSystemRenderFeature & UH_ENV_CUBE) == 0)
    {
        return 0;
    }
    
    // calculate sky tracing cone angle based on the confidence weight
    float4 SkyData = SampleSkyData(SceneWorldPos, SceneNormal, OutputCoord, false);
    float SkyConfidence = max(SkyData.w, 0.05f);
    float SkyConeAngle = lerp(cos(GMinSkyConeAngle * UH_DEG_TO_RAD), cos(GMaxSkyConeAngle * UH_DEG_TO_RAD), SkyConfidence);
    SkyConeAngle = acos(SkyConeAngle);
    
    // build TBN from the bent dir
    float3 SkyT, SkyB;
    BuildTBNFromNormal(SkyData.xyz, SkyT, SkyB);
    
    // halton uv and random rotation
    uint Phase = GetJitterPhase2D(OutputCoord);
    float2 UV = GRayOffset[Phase];
    float RandAngle = CoordinateToHash(OutputCoord + Phase) * UH_PI * 2.0f;
    
    float3 ConeDir = ConvertUVToConePos(UV, SkyConeAngle);
    ConeDir = normalize(ConeDir.x * SkyT + ConeDir.y * SkyB + ConeDir.z * SkyData.xyz);
    ConeDir = RotateVectorByAxisAngle(ConeDir, SkyData.xyz, RandAngle);
            
    RayDesc SkyRay = (RayDesc) 0;
    SkyRay.Origin = SceneWorldPos + SceneNormal * RayGap;
    SkyRay.Direction = ConeDir;
    SkyRay.TMin = RayGap;
    SkyRay.TMax = max(max(GSceneExtent.x, GSceneExtent.y), GSceneExtent.z) * 2.0f;
    
    float SkyVisibility = 0.0f;
    if (dot(ConeDir, SceneNormal) > 0)
    {
        UHMinimalPayload Payload = (UHMinimalPayload) 0;
        TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, GRTMinimalHitGroupOffset, 0, GRTShadowMissShaderIndex, SkyRay, Payload);
        SkyVisibility = Payload.IsHit() ? 0.0f : 1.0f;
    }
    
    SkyVisibility *= saturate(dot(SkyData.xyz, SceneNormal));
    return float2(SkyVisibility, SkyConfidence);
}

float3 CalculateIndirectLighting(in UHDefaultPayload OldPayload
    , float2 ScreenUV
    , float3 ScenePos
    , float3 SceneNormal
    , float3 HitWorldPos
    , float SceneSmoothness
    , float MipLevel
    , float RayGap
    , uint2 PixelCoord
)
{
    // doing the same lighting as object pass except the indirect specular
    float3 Result = 0;
    bool bIsInsideScreen;
    float2 HitScreenUV = CalculateScreenUV(HitWorldPos, bIsInsideScreen);
    
    OldPayload.HitVertexNormal = normalize(OldPayload.HitVertexNormal);
    
    // light calculation, be sure to normalize normal vector before using it
    // the specular part can also be ignored as that's not necessary for indirect diffuse
    uint2 HitPixelCoord = uint2(HitScreenUV * GResolution.xy);
    uint TileX = CoordToTileX(HitPixelCoord.x);
    uint TileY = CoordToTileY(HitPixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    
    UHLightInfo IndirectLightInfo;
    IndirectLightInfo.Diffuse = OldPayload.HitDiffuse.rgb;
    IndirectLightInfo.WorldPos = HitWorldPos;
    IndirectLightInfo.AttenNoise = GetAttenuationNoise(HitPixelCoord.xy) * 0.1f;
    IndirectLightInfo.Normal = OldPayload.HitVertexNormal;
    
    // calc indirect atten = inverse square distance atten x (1 - smoothness) x final scale
    // as high smoothness surfaces will reflect more than indirect diffuse
    float IndirectAtten = saturate((GIndirectLightFadeDistance - OldPayload.HitT) / GIndirectLightFadeDistance);
    IndirectAtten *= IndirectAtten;
    IndirectAtten *= (1.0f - SceneSmoothness);
    IndirectAtten *= GIndirectLightScale;
    
    // dir indirect diffuse
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
        {
            UHDirectionalLight DirLight = UHDirLights[Ldx];
            UHBRANCH
            if (DirLight.bIsEnabled)
            {
                float HitDist = 0;
                float Shadow = 1.0f;
            
                TraceDiretionalShadow(TLAS, DirLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel, Shadow, HitDist);
                
                // setup shadow mask and calculate dir light, true for skipping specular term as diffuse is the only concern here
                IndirectLightInfo.ShadowMask = Shadow;
                Result += CalculateDirLight(DirLight, IndirectLightInfo, true) * IndirectAtten;
            }
        }
    }
    
    // point indirect light
    if (GNumPointLights > 0)
    {
        uint TileOffset = GetPointLightOffset(TileIndex);
        uint PointLightCount = PointLightList.Load(TileOffset);
        TileOffset += 4;
        
        // fetch tiled point light if the hit point is inside screen, otherwise need to lookup from instance lights
        UHBRANCH
        if (bIsInsideScreen && PointLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
            {
                uint PointLightIdx = PointLightList.Load(TileOffset);
                TileOffset += 4;
       
                UHPointLight PointLight = UHPointLights[PointLightIdx];
                UHBRANCH
                if (PointLight.bIsEnabled)
                {
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TracePointShadow(TLAS, PointLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    
                    IndirectLightInfo.ShadowMask = Shadow;
                    Result += CalculatePointLight(PointLight, IndirectLightInfo, true) * IndirectAtten;
                }
            }
        }
        else
        {
            UHInstanceLights Lights = InstanceLights[OldPayload.HitInstanceIndex];
            for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
            {
                uint LightIndex = Lights.PointLightIndices[Ldx];
                if (LightIndex == ~0)
                {
                    break;
                }
            
                UHPointLight PointLight = UHPointLights[LightIndex];
                UHBRANCH
                if (PointLight.bIsEnabled)
                {
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TracePointShadow(TLAS, PointLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    
                    IndirectLightInfo.ShadowMask = Shadow;
                    Result += CalculatePointLight(PointLight, IndirectLightInfo, true) * IndirectAtten;
                }
            }
        }
    }
    
    // spot indirect light
    if (GNumSpotLights > 0)
    {
        uint TileOffset = GetSpotLightOffset(TileIndex);
        uint SpotLightCount = SpotLightList.Load(TileOffset);
        TileOffset += 4;
        
        UHBRANCH
        if (bIsInsideScreen && SpotLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < SpotLightCount; Ldx++)
            {
                uint SpotLightIdx = SpotLightList.Load(TileOffset);
                TileOffset += 4;
        
                UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
                UHBRANCH
                if (SpotLight.bIsEnabled)
                {
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TraceSpotShadow(TLAS, SpotLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    
                    IndirectLightInfo.ShadowMask = Shadow;
                    Result += CalculateSpotLight(SpotLight, IndirectLightInfo, true) * IndirectAtten;
                }
            }
        }
        else
        {
            UHInstanceLights Lights = InstanceLights[OldPayload.HitInstanceIndex];
            for (uint Ldx = 0; Ldx < GMaxPointSpotLightPerInstance; Ldx++)
            {
                uint LightIndex = Lights.SpotLightIndices[Ldx];
                if (LightIndex == ~0)
                {
                    break;
                }

                UHSpotLight SpotLight = UHSpotLights[LightIndex];
                UHBRANCH
                if (SpotLight.bIsEnabled)
                {
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TraceSpotShadow(TLAS, SpotLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    
                    IndirectLightInfo.ShadowMask = Shadow;
                    Result += CalculateSpotLight(SpotLight, IndirectLightInfo, true) * IndirectAtten;
                }
            }
        }
    }
    
    {
        float BouncedSkyLightOcclusion = TraceSkyVisibility(PixelCoord, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, 0, RayGap).r;
        Result += ShadeSH9(IndirectLightInfo.Diffuse, float4(IndirectLightInfo.Normal, 1.0f), BouncedSkyLightOcclusion) * IndirectAtten;
    }
    
    // prevent NaN and output, though it's weird that NaN happened
    return -min(-Result, 0.0f);
}

[shader("raygeneration")]
void RTIndirectLightRayGen()
{
    uint2 OutputCoord = DispatchRaysIndex().xy;
    OutDiffuse[OutputCoord] = 0;
    OutSkyData[OutputCoord] = 1;
    
    uint2 PixelCoord = OutputCoord * GIndirectDownsampleFactor;
    float2 ScreenUV = float2(PixelCoord + 0.5f) * GResolution.zw;
    
    float SceneDepth = SceneBuffers[3].SampleLevel(PointClampped, ScreenUV, 0).r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    float4 OpaqueBump = SceneBuffers[1].SampleLevel(LinearClampped, ScreenUV, 0);

    bool bTraceThisPixel = OpaqueBump.a > 0;
    if (!bTraceThisPixel)
    {
        return;
    }
    
    float Smoothness = SceneBuffers[2].SampleLevel(LinearClampped, ScreenUV, 0).a;
    
    // Mip Level
    float MipRate = SceneMipTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GILMipBias;
    
    // Scene normal
    uint PackedSceneData = SceneDataTexture[PixelCoord];
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
        SceneNormal = OpaqueBump.xyz;
        SceneNormal = DecodeNormal(SceneNormal);
    }
    
    // small ray rap applied for avoiding self-occlusion
    float RayGap = lerp(0.05f, GRTGapMax * 2, saturate(MipRate * RT_MIPRATESCALE));
    float3 Result = 0;
   
    // ------------ the first phase - trace sky visibility
    OutSkyData[OutputCoord] = TraceSkyVisibility(OutputCoord, SceneWorldPos, SceneNormal, ScreenUV, RayGap);
    
    // debugging
    if (false)
    {
        float4 SkyData = SampleSkyData(SceneWorldPos, SceneNormal, OutputCoord, false);
        OutDiffuse[OutputCoord] = float4(SkyData.xyz * 0.5f + 0.5f, 1);
        OutSkyData[OutputCoord] = SkyData.w;
        return;
    }
    
    // ------------ the second phase - indirect ray setup and trace for bounced diffuse
    float3 Tangent;
    float3 BiNormal;
    BuildTBNFromNormal(SceneNormal, Tangent, BiNormal);
    
    {
        RayDesc IndirectRay = (RayDesc) 0;
        IndirectRay.Origin = SceneWorldPos + SceneNormal * RayGap;
            
        // setup cone dir offset
        uint Phase = GetJitterPhase2D(OutputCoord);
        float RandAngle = CoordinateToHash(OutputCoord + Phase) * UH_PI * 2.0f;
        float3 ConeDir = ConvertUVToConePos(GRayOffset[Phase], GIndirectRayAngle * UH_DEG_TO_RAD);
        
        ConeDir = normalize(ConeDir.x * Tangent + ConeDir.y * BiNormal + ConeDir.z * SceneNormal);
        ConeDir = RotateVectorByAxisAngle(ConeDir, SceneNormal, RandAngle);
        IndirectRay.Direction = ConeDir;
    
        IndirectRay.TMin = RayGap;
        IndirectRay.TMax = GIndirectLightMaxTraceDist;

        UHDefaultPayload Payload = (UHDefaultPayload) 0;
        Payload.MipLevel = MipLevel;
        Payload.PayloadData |= PAYLOAD_ISINDIRECTLIGHT;
    
        // trace ray!
        TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, GRTDefaultMissShaderIndex, IndirectRay, Payload);
        
        if (Payload.IsHit())
        {
            float3 HitWorldPos = IndirectRay.Origin + IndirectRay.Direction * Payload.HitT;
            Result += CalculateIndirectLighting(Payload, ScreenUV, SceneWorldPos, SceneNormal
                    , HitWorldPos, Smoothness, MipLevel, RayGap, OutputCoord);
        }
        
        OutDiffuse[OutputCoord] = float4(Result, 1.0f);
    }
}

[shader("miss")]
void RTIndirectShadowMiss(inout UHMinimalPayload Payload)
{
    // if shadow ray missed
}

[shader("miss")]
void RTIndirectLightMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the result will just remain unchanged
}