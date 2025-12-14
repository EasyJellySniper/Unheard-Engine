// RayTracingShadow.hlsl - Shader for opaque shadow tracing
RaytracingAccelerationStructure TLAS : register(t1);

// out shadow data, r=hit distance, g=attenuation
RWTexture2DArray<float2> OutShadowData : register(u2);

// out shadow mask bits to tell whether to receive light
// for now this is 32-bit, which can trace 32 lights per pixel at max
RWTexture2D<uint> OutReceiveLightBits : register(u3);

#define UHGBUFFER_BIND t4
#define UHDIRLIGHT_BIND t5
#define UHPOINTLIGHT_BIND t6
#define UHSPOTLIGHT_BIND t7

ByteAddressBuffer PointLightList : register(t8);
ByteAddressBuffer SpotLightList : register(t9);

Texture2D SceneMipTexture : register(t10);

SamplerState PointClampped : register(s11);
SamplerState LinearClampped : register(s12);

#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"

void TraceShadow(uint2 PixelCoord, uint2 OutputCoord, float2 ScreenUV, float MipRate, float MipLevel)
{
    float SceneDepth = SceneBuffers[3].SampleLevel(PointClampped, ScreenUV, 0).r;

	UHBRANCH
    if (SceneDepth == 0.0f)
    {
        // early out if no depth
		return;
    }

	// reconstruct world position and get world normal
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);

	// reconstruct normal, simply sample from the texture
    float3 WorldNormal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, ScreenUV, 0).xyz);
	
	// ------------------------------------------------------------------------------------------ Directional Light Tracing
	// give a little gap for preventing self-shadowing, along the vertex normal direction
	// distant pixel needs higher TMin
    float Gap = lerp(GRTGapMin * 2, GRTGapMax * 2, saturate(MipRate * RT_MIPRATESCALE));

    uint ReceiveLightBit = 0;
    uint TraceCount = 0;
    uint SoftShadowCount = 0;

    for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];
        UHBRANCH
        if (DirLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            TraceDiretionalShadow(TLAS, DirLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            
            // output shadow to specific slice and set receive light bit
            if (SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                OutShadowData[uint3(OutputCoord, SoftShadowCount++)] = float2(HitDist, Atten);
            }
            
            ReceiveLightBit |= (Atten > 0.0f) ? 1u << TraceCount : 0;
            TraceCount++;
        }
    }
    
	// ------------------------------------------------------------------------------------------ Point Light Tracing
    uint2 TileCoordinate = PixelCoord.xy;
    uint TileX = CoordToTileX(TileCoordinate.x);
    uint TileY = CoordToTileY(TileCoordinate.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint LightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
	
    uint ClosestPointLightIndex = GetClosestPointLightIndex(PointLightList, TileIndex, WorldPos);
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint PointLightIdx = PointLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        UHBRANCH
        if (PointLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            TracePointShadow(TLAS, PointLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            
            // soft shadow for the closest point light 
            if (ClosestPointLightIndex == PointLightIdx && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                OutShadowData[uint3(OutputCoord, SoftShadowCount++)] = float2(HitDist, Atten);
            }
            
            // accumulate atten and max dist
            ReceiveLightBit |= (Atten > 0.0f) ? 1u << TraceCount : 0;
            TraceCount++;
        }
    }
	
	// ------------------------------------------------------------------------------------------ Spot Light Tracing
    TileOffset = GetSpotLightOffset(TileIndex);
    LightCount = SpotLightList.Load(TileOffset);
    TileOffset += 4;
	
    uint ClosestSpotLightIndex = GetClosestSpotLightIndex(SpotLightList, TileIndex, WorldPos);
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint SpotLightIdx = SpotLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        UHBRANCH
        if (SpotLight.bIsEnabled)
        {
            float HitDist = 0;
            float Atten = 1.0f;
            TraceSpotShadow(TLAS, SpotLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, HitDist);
            
            // soft shadow for the closest point light 
            if (ClosestSpotLightIndex == SpotLightIdx && SoftShadowCount < GMaxSoftShadowLightsPerPixel)
            {
                OutShadowData[uint3(OutputCoord, SoftShadowCount++)] = float2(HitDist, Atten);
            }
            
            // accumulate atten and max dist
            ReceiveLightBit |= (Atten > 0.0f) ? 1u << TraceCount : 0;
            TraceCount++;
        }
    }
    
    OutReceiveLightBits[OutputCoord] = ReceiveLightBit;
}

[shader("raygeneration")]
void RTShadowRayGen() 
{
    uint2 OutputCoord = DispatchRaysIndex().xy;
    for (uint Idx = 0; Idx < GMaxSoftShadowLightsPerPixel; Idx++)
    {
        OutShadowData[uint3(OutputCoord, Idx)] = float2(0.0f, 1.0f);
    }
    OutReceiveLightBits[OutputCoord] = 0;
	
	// early return if no lights
	UHBRANCH
	if (!HasLighting())
	{
		return;
	}

    // PixelCoord to UV
    uint2 PixelCoord = OutputCoord * GResolution.xy * GShadowResolution.zw;
	float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;

	// calculate mip level before ray tracing kicks off
    float MipRate = SceneMipTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;

    // trace direct light
    TraceShadow(PixelCoord, OutputCoord, ScreenUV, MipRate, MipLevel);
}

[shader("miss")]
void RTShadowMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}