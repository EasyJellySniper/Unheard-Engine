#define UHPOINTLIGHT_BIND t1
#define UHSPOTLIGHT_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "UHLightCommon.hlsli"

// output results, one for opaque objects, another for translucent objects
// the list will be reset every frame before this shader gets called
RWByteAddressBuffer OutPointLightList : register(u3);
RWByteAddressBuffer OutPointLightListTrans : register(u4);
RWByteAddressBuffer OutSpotLightList : register(u5);
RWByteAddressBuffer OutSpotLightListTrans : register(u6);
Texture2D DepthTexture : register(t7);
Texture2D TransDepthTexture : register(t8);

groupshared uint GMinDepth;
groupshared uint GMaxDepth;
groupshared uint GTileLightCount;

// calc view space frustum
void CalcFrustumPlanes(uint TileX, uint TileY, float MinZ, float MaxZ, out float4 Plane[6])
{
	// tile position in screen space
    float X = TileX * UHLIGHTCULLING_TILE;
    float Y = TileY * UHLIGHTCULLING_TILE;
    float RX = (TileX + 1) * UHLIGHTCULLING_TILE;
    float RY = (TileY + 1) * UHLIGHTCULLING_TILE;

	// frustum corner (at far plane) - LB RB LT RT, do not set z as zero since system uses infinite far z
    Plane[0] = float4(X, Y, 0.0001f, 1.0f);
    Plane[1] = float4(RX, Y, 0.0001f, 1.0f);
    Plane[2] = float4(X, RY, 0.0001f, 1.0f);
    Plane[3] = float4(RX, RY, 0.0001f, 1.0f);

	// near far
    Plane[4] = float4(0, 0, MinZ, 1.0f);
    Plane[5] = float4(0, 0, MaxZ, 1.0f);

	UHUNROLL
    for (uint Idx = 0; Idx < 6; Idx++)
    {
		// convert corners to view position, since system uses infinite far Z, do not set calculate position with 0 depth
        Plane[Idx].xyz = ComputeViewPositionFromDeviceZ(Plane[Idx].xy * UHLIGHTCULLING_UPSCALE, Plane[Idx].z);
    }

	// update min/max eye-z
    MinZ = Plane[4].z;
    MaxZ = Plane[5].z;

	// dir from camera to corners
    float3 Corners[4];
	UHUNROLL
    for (Idx = 0; Idx < 4; Idx++)
    {
        Corners[Idx].xyz = Plane[Idx].xyz;
    }

	// plane order: Left, Right, Bottom, Top, Near, Far
	// note the cross order: top-to-bottom left-to-right
    Plane[0].xyz = normalize(cross(Corners[2], Corners[0]));
    Plane[0].w = 0;

    Plane[1].xyz = -normalize(cross(Corners[3], Corners[1])); // flip so right plane point inside frustum
    Plane[1].w = 0;

    Plane[2].xyz = normalize(cross(Corners[0], Corners[1]));
    Plane[2].w = 0;

    Plane[3].xyz = -normalize(cross(Corners[2], Corners[3])); // flip so top plane point inside frustum
    Plane[3].w = 0;

    Plane[4].xyz = float3(0, 0, 1);
    Plane[4].w = -MinZ;

    Plane[5].xyz = float3(0, 0, -1);
    Plane[5].w = MaxZ;
}

void CalcFrustumCorners(uint TileX, uint TileY, float MinZ, float MaxZ, out float3 Corners[8])
{
    // tile position in screen space
    float X = TileX * UHLIGHTCULLING_TILE;
    float Y = TileY * UHLIGHTCULLING_TILE;
    float RX = (TileX + 1) * UHLIGHTCULLING_TILE;
    float RY = (TileY + 1) * UHLIGHTCULLING_TILE;

	// frustum corners - LB RB LT RT, at the near and far
    Corners[0] = float3(X, Y, MinZ);
    Corners[1] = float3(RX, Y, MinZ);
    Corners[2] = float3(X, RY, MinZ);
    Corners[3] = float3(RX, RY, MinZ);
    Corners[4] = float3(X, Y, MaxZ);
    Corners[5] = float3(RX, Y, MaxZ);
    Corners[6] = float3(X, RY, MaxZ);
    Corners[7] = float3(RX, RY, MaxZ);
    
    // note that these are world positions
    UHUNROLL
    for (int Idx = 0; Idx < 8; Idx++)
    {
        Corners[Idx] = ComputeWorldPositionFromDeviceZ(Corners[Idx].xy * UHLIGHTCULLING_UPSCALE, Corners[Idx].z);
    }
}

void CullPointLight(uint3 Gid, uint GIndex, float Depth, bool bForTranslucent)
{
    // init group shared value
    if (GIndex == 0)
    {
        GMinDepth = ~0;
        GMaxDepth = 0;
        GTileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    // find min-max depth
    if (Depth > 0.0f)
    {
        uint DepthUInt = asuint(Depth);
        InterlockedMin(GMinDepth, DepthUInt);
        InterlockedMax(GMaxDepth, DepthUInt);
    }
	GroupMemoryBarrierWithGroupSync();
    
    uint TileIdx = Gid.x + Gid.y * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIdx);
    
    // convert depth to float and calculate the bounding box
    float MinDepth = asfloat(GMinDepth);
    float MaxDepth = asfloat(GMaxDepth);
    
    UHBRANCH
    if (GMinDepth == ~0 && GMaxDepth == 0)
    {
        // no valid depth in this tile, simple return
        return;
    }
    
    // don't let depth value be 0 as system uses infinite reversed-z on the far plane, 0 can causes wrong world position
    // also give a small difference if two depth are very close
    MinDepth = lerp(MinDepth, 0.0001f, MinDepth == 0.0f);
    MaxDepth = lerp(MaxDepth, 0.0001f, MaxDepth == 0.0f);
    MaxDepth = lerp(MaxDepth, MinDepth + 0.0001f, (MaxDepth - MinDepth) < UH_FLOAT_EPSILON);
    
    float4 TileFrustum[6];
    CalcFrustumPlanes(Gid.x, Gid.y, MaxDepth, MinDepth, TileFrustum);
    
    for (uint LightIdx = GIndex; LightIdx < GNumPointLights; LightIdx += UHLIGHTCULLING_TILE * UHLIGHTCULLING_TILE)
    {
        UHPointLight PointLight = UHPointLights[LightIdx];
        UHBRANCH
        if (!PointLight.bIsEnabled)
        {
            continue;
        }
        
        float3 PointLightViewPos = WorldToViewPos(PointLight.Position);
        
        bool bIsOverlapped = SphereIntersectsFrustum(float4(PointLightViewPos, PointLight.Radius), TileFrustum);
        if (bIsOverlapped)
        {
            uint StoreIdx = 0;
            InterlockedAdd(GTileLightCount, 1, StoreIdx);
            
            // discard the result that exceeds the max point light per tile
            if (StoreIdx < GMaxPointLightPerTile)
            {
                if (!bForTranslucent)
                {
                    OutPointLightList.Store(TileOffset + 4 + StoreIdx * 4, LightIdx);
                }
                else
                {
                    OutPointLightListTrans.Store(TileOffset + 4 + StoreIdx * 4, LightIdx);
                }
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

	// output final result, the first 4 bytes stores the number of point light per tile
    // then it stores the point light indices that overlapped with this tile
    if (GIndex == 0)
    {
        if (!bForTranslucent)
        {
            OutPointLightList.Store(TileOffset, min(GTileLightCount, GMaxPointLightPerTile));
        }
        else
        {
            OutPointLightListTrans.Store(TileOffset, min(GTileLightCount, GMaxPointLightPerTile));
        }
    }
}

bool IsTileWithinSpotAngle(const float3 Pos, const float3 Dir, const float Angle, const float3 Corners[8])
{
    // as long as ONE corner of a frustum is inside the spotlight range, return true
    UHUNROLL
    for (int Idx = 0; Idx < 8; Idx++)
    {
        const float3 LightToCorner = normalize(Corners[Idx] - Pos);
        const float LightCornerAngle = acos(dot(LightToCorner, Dir));
        
        // give it a small tolerance when checking the angle
        // this prevents the artifacts on the edge of the spot light
        UHBRANCH
        if (LightCornerAngle <= Angle + 1.0f)
        {
            return true;
        }
    }
    
    return false;
}

void CullSpotLight(uint3 Gid, uint GIndex, float Depth, bool bForTranslucent)
{
    // init group shared value
    if (GIndex == 0)
    {
        GMinDepth = ~0;
        GMaxDepth = 0;
        GTileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    // find min-max depth
    if (Depth > 0.0f)
    {
        uint DepthUInt = asuint(Depth);
        InterlockedMin(GMinDepth, DepthUInt);
        InterlockedMax(GMaxDepth, DepthUInt);
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint TileIdx = Gid.x + Gid.y * GLightTileCountX;
    uint TileOffset = GetSpotLightOffset(TileIdx);
    
    // convert depth to float and calculate the bounding box
    float MinDepth = asfloat(GMinDepth);
    float MaxDepth = asfloat(GMaxDepth);
    
    UHBRANCH
    if (GMinDepth == ~0 && GMaxDepth == 0)
    {
        // no valid depth in this tile, simple return
        return;
    }
    
    // don't let depth value be 0 as system uses infinite reversed-z on the far plane, 0 can causes wrong world position
    // also give a small difference if two depth are very close
    MinDepth = lerp(MinDepth, 0.0001f, MinDepth == 0.0f);
    MaxDepth = lerp(MaxDepth, 0.0001f, MaxDepth == 0.0f);
    MaxDepth = lerp(MaxDepth, MinDepth + 0.0001f, (MaxDepth - MinDepth) < UH_FLOAT_EPSILON);
    
    float4 TileFrustum[6];
    CalcFrustumPlanes(Gid.x, Gid.y, MaxDepth, MinDepth, TileFrustum);
    
    float3 TileCornersWorld[8];
    CalcFrustumCorners(Gid.x, Gid.y, MaxDepth, MinDepth, TileCornersWorld);
    
    for (uint LightIdx = GIndex; LightIdx < GNumSpotLights; LightIdx += UHLIGHTCULLING_TILE * UHLIGHTCULLING_TILE)
    {
        UHSpotLight SpotLight = UHSpotLights[LightIdx];
        UHBRANCH
        if (!SpotLight.bIsEnabled)
        {
            continue;
        }
        
        float3 SpotLightViewPos = WorldToViewPos(SpotLight.Position);
        
        bool bIsOverlapped = SphereIntersectsFrustum(float4(SpotLightViewPos, SpotLight.Radius), TileFrustum)
            && IsTileWithinSpotAngle(SpotLight.Position, SpotLight.Dir, SpotLight.Angle, TileCornersWorld);
        
        if (bIsOverlapped)
        {
            uint StoreIdx = 0;
            InterlockedAdd(GTileLightCount, 1, StoreIdx);
            
            // discard the result that exceeds the max point light per tile
            if (StoreIdx < GMaxSpotLightPerTile)
            {
                if (!bForTranslucent)
                {
                    OutSpotLightList.Store(TileOffset + 4 + StoreIdx * 4, LightIdx);
                }
                else
                {
                    OutSpotLightListTrans.Store(TileOffset + 4 + StoreIdx * 4, LightIdx);
                }
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

	// output final result, the first 4 bytes stores the number of point light per tile
    // then it stores the point light indices that overlapped with this tile
    if (GIndex == 0)
    {
        if (!bForTranslucent)
        {
            OutSpotLightList.Store(TileOffset, min(GTileLightCount, GMaxSpotLightPerTile));
        }
        else
        {
            OutSpotLightListTrans.Store(TileOffset, min(GTileLightCount, GMaxSpotLightPerTile));
        }
    }
}

// this time the numthreads follows tile setting instead of regular thread group 2D definition
// note that it's culling at half resolution
[numthreads(UHLIGHTCULLING_TILE, UHLIGHTCULLING_TILE, 1)]
void LightCullingCS(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint GIndex : SV_GroupIndex)
{
    if (DTid.x * UHLIGHTCULLING_UPSCALE >= GResolution.x || DTid.y * UHLIGHTCULLING_UPSCALE >= GResolution.y)
    {
        // no need to evaluate out-of-range pixels
        return;
    }
    
    float Depth = 0;
    float TransDepth = 0;
    
    // the light culling is done with half-sized resolution, find the max depth within 2x2 box
    int Dx[4] = { 0, 1, 0, 1 };
    int Dy[4] = { 0, 0, 1, 1 };
    for (int I = 0; I < 4; I++)
    {
        int2 Pos = DTid.xy * UHLIGHTCULLING_UPSCALE + int2(Dx[I], Dy[I]);
        Pos = min(Pos, GResolution.xy - 1);
        
        Depth = max(DepthTexture[Pos].r, Depth);
        TransDepth = max(TransDepthTexture[Pos].r, TransDepth);
    }
    
    // cull point light for both opaque and translucent objects
    // point light culling uses a box-sphere test method, although it's not as precise as sphere-frustum test but faster
    UHBRANCH
    if (GNumPointLights > 0)
    {
        CullPointLight(Gid, GIndex, Depth, false);
        CullPointLight(Gid, GIndex, TransDepth, true);
    }
    
    // cull spot light for both opaque and translucent objects
    // uses a similar way as culling point light, but just considers the angle as well
    UHBRANCH
    if (GNumSpotLights > 0)
    {
        CullSpotLight(Gid, GIndex, Depth, false);
        CullSpotLight(Gid, GIndex, TransDepth, true);
    }
}