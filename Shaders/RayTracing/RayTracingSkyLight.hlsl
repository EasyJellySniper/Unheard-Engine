// RayTracingSkyLight.hlsl - Shader for sky light tracing. Which is also a part of RTIL.
// This can be executed on async compute queue as there has no other dependencies.
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "UHRTCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);
// xyz = bent sky direction that tends to reached the sky, w = confidence (escaped / total)
RWTexture3D<float4> OutSkyData : register(u2);
RWTexture3D<float> OutSkyDiscoverAngle : register(u3);

// consider moving some parameters to the settings
static const uint GSkySampleCountX = 4;
static const uint GSkySampleCountY = 4;
static const float GInvisibleVoxelCullDistance = 10.0f;
static const float3 GWorldUp = float3(0, 1, 0);
static const float GRayGap = 0.01f;
static const float GMaxDiscoverPhase = 32.0f;
static const uint GNumOfFramesToUpdateVoxels = 4;

// whether a voxel is inside screen
bool IsVoxelInsideScreen(float3 VoxelPos, float VoxelExtent)
{
    bool bInsideScreen = false;
    
    // simple 8 corners check
    const float3 Corners[8] = 
    {
        float3(1,1,1),
        float3(-1,1,1),
        float3(1,-1,1),
        float3(-1,-1,1),
        float3(-1,1,-1),
        float3(1,1,-1),
        float3(-1,-1,-1),
        float3(1,-1,-1)
    };
    
    UHUNROLL
    for (uint I = 0; I < 8; I++)
    {
        CalculateScreenUV(VoxelPos + Corners[I] * VoxelExtent * 2.0f, bInsideScreen);
        if (bInsideScreen)
        {
            // early out as long as a corner is visible
            return true;
        }
    }
    
    return false;
}

uint GetVoxelIndex(uint3 VoxelCoord, out uint TotalVoxels)
{
    uint3 Dimension = DispatchRaysDimensions();
    TotalVoxels = Dimension.x * Dimension.y * Dimension.z;
    return (VoxelCoord.z * Dimension.x * Dimension.y) + (VoxelCoord.y * Dimension.x) + VoxelCoord.x;
}

bool ShouldUpdateVoxel(uint3 VoxelCoord, float3 VoxelPos)
{
    // update voxels when -
    // (1) It is inside screen and match the phase based on frame index
    // (2) Close to the camera
    uint TotalVoxels;
    uint VoxelIndex = GetVoxelIndex(VoxelCoord, TotalVoxels);
    uint Phase = GFrameNumber % GNumOfFramesToUpdateVoxels;
    bool bPhaseMatch = Phase == (VoxelIndex % GNumOfFramesToUpdateVoxels);
    
    return (bPhaseMatch && IsVoxelInsideScreen(VoxelPos, 0.5f)) || length(VoxelPos - GCameraPos) < GInvisibleVoxelCullDistance;
}

[shader("raygeneration")]
void RTSkyLightRayGen()
{
    uint3 OutputCoord = DispatchRaysIndex();
    float3 SceneBoundMin = GSceneCenter - GSceneExtent;
    float3 VoxelPos = float3(OutputCoord) + SceneBoundMin + 0.5f;
    
    bool bTraceThisVoxel = ShouldUpdateVoxel(OutputCoord, VoxelPos);
    if (!bTraceThisVoxel)
    {
        return;
    }

    RayDesc SkyRay = (RayDesc)0;
    SkyRay.Origin = VoxelPos + GWorldUp * GRayGap;
    SkyRay.TMin = GRayGap;
    SkyRay.TMax = max(max(GSceneExtent.x, GSceneExtent.y), GSceneExtent.z) * 2.0f;
    
    float3 AvgDir = 0.0f;
    float EscapedCount = 0.0f;
    
    // apply randomness to the sky ray direction, also considers the accmulated phase
    float Phase = OutSkyDiscoverAngle[OutputCoord];
    
    float3 RandAxis = normalize(float3(CoordinateToHash(VoxelPos + 1 + Phase)
        , CoordinateToHash(VoxelPos + 2 + Phase)
        , CoordinateToHash(VoxelPos + 3 + Phase)));
    
    float RandAngle = CoordinateToHash(VoxelPos + 4 + Phase) * UH_PI * 2.0f;
    
    for (uint I = 0; I < GSkySampleCountX; I++)
    {
        for (uint J = 0; J < GSkySampleCountY; J++)
        {
            float2 UV = (float2(I, J) + 0.5f) / float2(GSkySampleCountX, GSkySampleCountY);
            SkyRay.Direction = ConvertUVToSpherePos(UV);
            SkyRay.Direction = RotateVectorByAxisAngle(SkyRay.Direction, RandAxis, RandAngle);
            SkyRay.Direction = normalize(SkyRay.Direction);
            
            UHMinimalPayload Payload = (UHMinimalPayload)0;
            TraceRay(TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, GRTMinimalHitGroupOffset, 0, GRTShadowMissShaderIndex, SkyRay, Payload);
            
            if (!Payload.IsHit())
            {
                // count escaped rays with weight
                float Weight = max(dot(SkyRay.Direction, GWorldUp), 0.1f);
                AvgDir += SkyRay.Direction * Weight;
                EscapedCount++;
            }
        }
    }
    
    // average dir and confidence
    AvgDir = EscapedCount > 0 ? normalize(AvgDir) : 0;
    float Confidence = EscapedCount / float(GSkySampleCountX * GSkySampleCountY);
    
    OutSkyData[OutputCoord] = float4(AvgDir, Confidence);
    
    if (Confidence == 0.0f)
    {
        // if no ray escaped, accumulate phase value so the voxel will try to discover with a new rotation
        // max up to 32 trials
        OutSkyDiscoverAngle[OutputCoord] = fmod(Phase + 1.0f, GMaxDiscoverPhase);
    }
}

[shader("miss")]
void RTSkyLightMiss(inout UHMinimalPayload Payload)
{
    // if shadow ray missed
}