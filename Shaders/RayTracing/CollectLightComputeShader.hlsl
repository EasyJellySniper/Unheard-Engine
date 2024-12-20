// shader to collect light for ray tracing instance use
#define UHPOINTLIGHT_BIND t2
#define UHSPOTLIGHT_BIND t3
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHMeshShaderCommon.hlsli"

// each instance will hold point/spot light indices that intersects it
RWStructuredBuffer<UHInstanceLights> Result : register(u1);
StructuredBuffer<ObjectConstants> UHObjects : register(t4);

groupshared uint GLightCount;

#if COLLECT_POINTLIGHT
// x to fetch instances, 1 instance = 1 group, if total instances are > 65K (rare) this needs a refactor
// y to fetch lights
[numthreads(1, UHTHREAD_GROUP2D_Y * UHTHREAD_GROUP2D_Y, 1)]
void CollectPointLightCS(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint GIndex : SV_GroupIndex)
{
    if (GNumPointLights == 0 || DTid.y >= GNumPointLights)
    {
        return;
    }

    uint InstanceID = Gid.x;
    uint LightIndex = DTid.y;
    
    if (GIndex == 0)
    {
        GLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    ObjectConstants Obj = UHObjects[InstanceID];
    UHPointLight Light = UHPointLights[LightIndex];
    if (!Light.bIsEnabled)
    {
        return;
    }

    float SqrDistToCam = dot(GCameraPos - Obj.GWorldPos, GCameraPos - Obj.GWorldPos);
    if (SqrDistToCam > GRTCullingDistance * GRTCullingDistance)
    {
        // early return if outside RT culling distance
        return;
    }

    if (BoxIntersectsSphere(Obj.GWorldPos - Obj.GBoundExtent, Obj.GWorldPos + Obj.GBoundExtent, Light.Position, Light.Radius))
    {
        uint StoreIdx;
        InterlockedAdd(GLightCount, 1, StoreIdx);
        
        if (StoreIdx < GMaxPointSpotLightPerInstance)
        {
            Result[InstanceID].PointLightIndices[StoreIdx] = LightIndex;
        }
    }
}
#endif

#if COLLECT_SPOTLIGHT
// almost the same as point light but it's for spot lights
[numthreads(1, UHTHREAD_GROUP2D_Y * UHTHREAD_GROUP2D_Y, 1)]
void CollectSpotLightCS(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint GIndex : SV_GroupIndex)
{
    if (GNumSpotLights == 0 || DTid.y >= GNumSpotLights)
    {
        return;
    }

    uint InstanceID = Gid.x;
    uint LightIndex = DTid.y;
    
    if (GIndex == 0)
    {
        GLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    ObjectConstants Obj = UHObjects[InstanceID];
    UHSpotLight Light = UHSpotLights[LightIndex];
    if (!Light.bIsEnabled)
    {
        return;
    }

    float SqrDistToCam = dot(GCameraPos - Obj.GWorldPos, GCameraPos - Obj.GWorldPos);
    if (SqrDistToCam > GRTCullingDistance * GRTCullingDistance)
    {
        // early return if outside RT culling distance
        return;
    }

    // transform the world bound to light-space bound
    float3 Center = Obj.GWorldPos;
    float3 Extent = Obj.GBoundExtent;
    
    float3 BoxMin = UH_FLOAT_MAX;
    float3 BoxMax = -UH_FLOAT_MAX;
    UHUNROLL
    for (int Idx = 0; Idx < 8; Idx++)
    {
        float3 Corner = GBoxOffset[Idx].xyz * Extent + Center;
        Corner = mul(float4(Corner, 1.0f), Light.WorldToLight).xyz;

        BoxMin = min(BoxMin, Corner);
        BoxMax = max(BoxMax, Corner);
    }
    
    // box-cone test
    if (BoxIntersectsConeFrustum(BoxMin, BoxMax, Light.Radius, Light.Angle))
    {
        uint StoreIdx;
        InterlockedAdd(GLightCount, 1, StoreIdx);
        
        if (StoreIdx < GMaxPointSpotLightPerInstance)
        {
            Result[InstanceID].SpotLightIndices[StoreIdx] = LightIndex;
        }
    }
}
#endif