#define UHPOINTLIGHT_BIND t1
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"

// output results, one for opaque objects, another for translucent objects
// the list will be reset every frame before this shader gets called
RWByteAddressBuffer OutPointLightList : register(u2);
RWByteAddressBuffer OutPointLightListTrans : register(u3);
Texture2D DepthTexture : register(t4);
Texture2D TransDepthTexture : register(t5);
SamplerState LinearClampped : register(s6);

groupshared uint GMinDepth;
groupshared uint GMaxDepth;
groupshared uint GTilePointLightCount;

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

void CullPointLight(uint3 Gid, uint GIndex, float Depth, bool bForTranslucent)
{
    // init group shared value
    if (GIndex == 0)
    {
        GMinDepth = ~0;
        GMaxDepth = 0;
        GTilePointLightCount = 0;
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
    
    uint TileIdx = Gid.x + Gid.y * UHLightTileCountX;
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
    
    for (uint LightIdx = GIndex; LightIdx < UHNumPointLights; LightIdx += UHLIGHTCULLING_TILE * UHLIGHTCULLING_TILE)
    {
        UHPointLight PointLight = UHPointLights[LightIdx];
        float3 PointLightViewPos = WorldToViewPos(PointLight.Position);
        
        bool bIsOverlapped = SphereIntersectsFrustum(float4(PointLightViewPos, PointLight.Radius), TileFrustum);
        if (bIsOverlapped)
        {
            uint StoreIdx = 0;
            InterlockedAdd(GTilePointLightCount, 1, StoreIdx);
            
            // discard the result that exceeds the max point light per tile
            if (StoreIdx < UHMaxPointLightPerTile)
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
            OutPointLightList.Store(TileOffset, min(GTilePointLightCount, UHMaxPointLightPerTile));
        }
        else
        {
            OutPointLightListTrans.Store(TileOffset, min(GTilePointLightCount, UHMaxPointLightPerTile));
        }
    }
}

// this time the numthreads follows tile setting instead of regular thread group 2D definition
// note that it's culling at half resolution
[numthreads(UHLIGHTCULLING_TILE, UHLIGHTCULLING_TILE, 1)]
void LightCullingCS(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint GIndex : SV_GroupIndex)
{
    if (DTid.x * UHLIGHTCULLING_UPSCALE >= UHResolution.x || DTid.y * UHLIGHTCULLING_UPSCALE >= UHResolution.y)
    {
        // no need to evaluate out-of-range pixels
        return;
    }
    
    // calc UV
    float2 UV = (DTid.xy * UHLIGHTCULLING_UPSCALE + 0.5f) * UHResolution.zw;
    float Depth = DepthTexture.SampleLevel(LinearClampped, UV, 0).r;
    float TransDepth = TransDepthTexture.SampleLevel(LinearClampped, UV, 0).r;
    
    // cull point light for both opaque and translucent objects
    // point light culling uses a box-sphere test method, although it's not as precise as sphere-frustum test but faster
    UHBRANCH
    if (UHNumPointLights > 0)
    {
        CullPointLight(Gid, GIndex, Depth, false);
        CullPointLight(Gid, GIndex, TransDepth, true);
    }
}