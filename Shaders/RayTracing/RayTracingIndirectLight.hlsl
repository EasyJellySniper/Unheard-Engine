// RayTracingIndirectLight.hlsl - Shader for opaque indirect light tracing
#define UHGBUFFER_BIND t5
#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#define UHSPOTLIGHT_BIND t8
#define SH9_BIND t9
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"
#include "../UHSphericalHamonricCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
RWTexture2D<float3> OutDiffuse[GNumOfIndirectFrames] : register(u2);
RWTexture2D<float> OutOcclusion[GNumOfIndirectFrames] : register(u3);

cbuffer RTIndirectConstants : register(b4)
{
    float GIndirectLightScale;
    float GIndirectLightFadeDistance;
    float GIndirectLightMaxTraceDist;
    float GOcclusionEndDistance;
    
    float GOcclusionStartDistance;
    int GIndirectDownsampleFactor;
    float GIndirectRayOffsetScale;
}

Texture2D SceneMipTexture : register(t10);
Texture2D<uint> SceneDataTexture : register(t11);
Texture2D SmoothSceneNormalTexture : register(t12);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t13);
ByteAddressBuffer PointLightList : register(t14);
ByteAddressBuffer SpotLightList : register(t15);

SamplerState PointClampped : register(s16);
SamplerState LinearClampped : register(s17);

static const uint GMaxNumOfIndirectLightRays = 4;
// cherry-picked halton sequence, can be changed in the future
static const float2 GRayOffset[GMaxNumOfIndirectLightRays] =
{
    float2(0.5f, 0.3333f) * 2.0f - 1.0f,
    float2(0.25f, 0.6666f) * 2.0f - 1.0f,
    float2(0.125f, 0.4444f) * 2.0f - 1.0f,
    float2(0.375f, 0.2222f) * 2.0f - 1.0f
};

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
    IndirectLightInfo.Normal = Payload.HitVertexNormal;
    
    // consider indirect occlusion for this pixel as well
    float AOThisPixel = saturate((Payload.HitT - GOcclusionStartDistance) / GOcclusionEndDistance);
    AOThisPixel = lerp(1.0f, AOThisPixel, Payload.HitAlpha);
    
    // calc indirect atten = inverse square distance atten x (1 - smoothness) x final scale
    // as high smoothness surfaces will reflect more than indirect diffuse
    float IndirectAtten = saturate((GIndirectLightFadeDistance - Payload.HitT) / GIndirectLightFadeDistance);
    IndirectAtten *= (1.0f - Payload.HitSpecular.a) * (1.0f - SceneSmoothness);
    IndirectAtten *= GIndirectLightScale;
    
    float3 ObjColorTerm = IndirectLightInfo.Diffuse * IndirectAtten;
    
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
                Result += DirLight.Color.rgb * ObjColorTerm * Shadow;
            }
        }
    }
    
    // need to calculate attenuation for positional lights
    float PositionalLightAtten = 0.0f;
    
    // point indirect light
    if (GNumPointLights > 0)
    {
        uint TileOffset = GetPointLightOffset(TileIndex);
        uint PointLightCount = PointLightList.Load(TileOffset);
        TileOffset += 4;
        
        // fetch tiled point light if the hit point is inside screen, otherwise need to lookup from instance lights
        UHBRANCH
        if (Payload.IsInsideScreen && PointLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
            {
                uint PointLightIdx = PointLightList.Load(TileOffset);
                TileOffset += 4;
       
                UHPointLight PointLight = UHPointLights[PointLightIdx];
                UHBRANCH
                if (PointLight.bIsEnabled)
                {
                    PositionalLightAtten = CalculatePointLightAttenuation(PointLight.Radius, IndirectLightInfo.AttenNoise
                        , ScenePos - PointLight.Position);
                    
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TracePointShadow(TLAS, PointLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    Result += PointLight.Color.rgb * ObjColorTerm * PositionalLightAtten * Shadow;
                }
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
                UHBRANCH
                if (PointLight.bIsEnabled)
                {
                    PositionalLightAtten = CalculatePointLightAttenuation(PointLight.Radius, IndirectLightInfo.AttenNoise
                        , ScenePos - PointLight.Position);
                    
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TracePointShadow(TLAS, PointLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    Result += PointLight.Color.rgb * ObjColorTerm * PositionalLightAtten * Shadow;
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
        if (Payload.IsInsideScreen && SpotLightCount > 0)
        {
            for (uint Ldx = 0; Ldx < SpotLightCount; Ldx++)
            {
                uint SpotLightIdx = SpotLightList.Load(TileOffset);
                TileOffset += 4;
        
                UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
                UHBRANCH
                if (SpotLight.bIsEnabled)
                {
                    PositionalLightAtten = CalculateSpotLightAttenuation(SpotLight.Radius, IndirectLightInfo.AttenNoise
                        , ScenePos - SpotLight.Position
                        , SpotLight.Dir, SpotLight.Angle, SpotLight.InnerAngle);
                    
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TraceSpotShadow(TLAS, SpotLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    Result += SpotLight.Color.rgb * ObjColorTerm * PositionalLightAtten * Shadow;
                }
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
                UHBRANCH
                if (SpotLight.bIsEnabled)
                {
                    PositionalLightAtten = CalculateSpotLightAttenuation(SpotLight.Radius, IndirectLightInfo.AttenNoise
                        , ScenePos - SpotLight.Position
                        , SpotLight.Dir, SpotLight.Angle, SpotLight.InnerAngle);
                    
                    float HitDist = 0;
                    float Shadow = 1.0f;
                    TraceSpotShadow(TLAS, SpotLight, IndirectLightInfo.WorldPos, IndirectLightInfo.Normal, RayGap, MipLevel
                        , Shadow, HitDist);
                    Result += SpotLight.Color.rgb * ObjColorTerm * PositionalLightAtten * Shadow;
                }
            }
        }
    }
    
    // indirect light through the object that receives sky light
    // this didn't shoot another ray to check whether hit position can receive sky light by now, as a perf hack
    // just let the current position occlusion to dim it
    Result += ShadeSH9(IndirectLightInfo.Diffuse.rgb, float4(IndirectLightInfo.Normal, 1.0f), IndirectAtten);
    
    return float4(Result, AOThisPixel);
}

[shader("raygeneration")]
void RTIndirectLightRayGen()
{
    // the system will now collect the tracing result with 4 samples
    // the tracings are distributed to 2 frames
    // E.g. 4 samples = 2 rays per frame
    uint OutputIndex = GFrameNumber % GNumOfIndirectFrames;
    uint2 OutputCoord = DispatchRaysIndex().xy;
    OutDiffuse[OutputIndex][OutputCoord] = float3(0, 0, 0);
    OutOcclusion[OutputIndex][OutputCoord] = 1;
    
    uint2 PixelCoord = OutputCoord * GIndirectDownsampleFactor;
    float2 ScreenUV = float2(PixelCoord) * GResolution.zw;
    
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
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;
    
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
    
    // Small ray rap applied for avoiding self-occlusion
    float RayGap = lerp(0.05f, GRTGapMax * 2, saturate(MipRate * RT_MIPRATESCALE));
    float4 Result = 0;
    float HitT = 0.0f;
    
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
                RayDir += GIndirectRayOffsetScale * float3(0, GRayOffset[OffsetIndex]);
            }
            else if (ClosestAxis < 4)
            {
                // y is the forward axis, offset xz
                RayDir += GIndirectRayOffsetScale * float3(GRayOffset[OffsetIndex].x, 0, GRayOffset[OffsetIndex].y);
            }
            else
            {
                // z is the forward axis, offset xy
                RayDir += GIndirectRayOffsetScale * float3(GRayOffset[OffsetIndex], 0);
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
        
        HitT = max(Payload.HitT, HitT);
    }
    
    // avg the result and output
    OutDiffuse[OutputIndex][OutputCoord] = Result.rgb / float(GNumOfIndirectFrames);
    OutOcclusion[OutputIndex][OutputCoord] = Result.a / float(GNumOfIndirectFrames);
}

[shader("miss")]
void RTIndirectLightMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the result will just remain unchanged
}