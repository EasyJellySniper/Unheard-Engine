#define UHDIRLIGHT_BIND t3
#define UHPOINTLIGHT_BIND t4
#define UHSPOTLIGHT_BIND t5
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"

RaytracingAccelerationStructure TLAS : register(t1);

// this pass will store result as a float2 buffer (max hit distance, attenuation)
// in soften pass, it will output to corresponding RT shadow targets
RWTexture2D<float2> OutShadowResult : register(u2);

ByteAddressBuffer PointLightList : register(t6);
ByteAddressBuffer PointLightListTrans : register(t7);
ByteAddressBuffer SpotLightList : register(t8);
ByteAddressBuffer SpotLightListTrans : register(t9);

Texture2D MixedMipTexture : register(t10);
Texture2D MixedDepthTexture : register(t11);
Texture2D MixedVertexNormalTexture : register(t12);
SamplerState PointSampler : register(s13);
SamplerState LinearSampler : register(s14);

// both opaque and translucent shadow are traced in this function
void TraceShadow(uint2 PixelCoord, float2 ScreenUV, float MipRate, float MipLevel)
{
    // The MixedDepthTexture contains both opaque/translucent depth
    float SceneDepth = MixedDepthTexture.SampleLevel(PointSampler, ScreenUV, 0).r;

	UHBRANCH
    if (SceneDepth == 0.0f)
    {
        // early out if no depth
		return;
    }

	// reconstruct world position and get world normal
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);

	// reconstruct normal, simply sample from the texture
    // likewise,  The MixedVertexNormalTexture contains both opaque/translucent's vertex normal
    float4 VertexNormalData = MixedVertexNormalTexture.SampleLevel(PointSampler, ScreenUV, 0);
    float3 WorldNormal = DecodeNormal(VertexNormalData.xyz);
	
	// ------------------------------------------------------------------------------------------ Directional Light Tracing
	// give a little gap for preventing self-shadowing, along the vertex normal direction
	// distant pixel needs higher TMin
    float Gap = lerp(0.01f, 0.05f, saturate(MipRate * RT_MIPRATESCALE));
	float MaxDist = 0;
    
    // always assume shadowed and only acclumate value when it's not hitting
	float Atten = 0.0f;

	for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
	{
		// shoot ray from world pos to light dir
		UHDirectionalLight DirLight = UHDirLights[Ldx];
        TraceDiretionalShadow(TLAS, DirLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, MaxDist);
    }
    
    // evaluate if it's translucent or not from VertexNormalData.w
    bool bIsTranslucent = VertexNormalData.w > 0;
    
	// ------------------------------------------------------------------------------------------ Point Light Tracing
    uint2 TileCoordinate = PixelCoord.xy;
    uint TileX = CoordToTileX(TileCoordinate.x);
    uint TileY = CoordToTileY(TileCoordinate.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint LightCount = (bIsTranslucent) ? PointLightListTrans.Load(TileOffset) : PointLightList.Load(TileOffset);
    TileOffset += 4;
	
    float3 LightToWorld;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint PointLightIdx = (bIsTranslucent) ? PointLightListTrans.Load(TileOffset) : PointLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        TracePointShadow(TLAS, PointLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, MaxDist);
    }
	
	// ------------------------------------------------------------------------------------------ Spot Light Tracing
    TileOffset = GetSpotLightOffset(TileIndex);
    LightCount = (bIsTranslucent) ? SpotLightListTrans.Load(TileOffset) : SpotLightList.Load(TileOffset);
    TileOffset += 4;
	
    for (Ldx = 0; Ldx < LightCount; Ldx++)
    {
        uint SpotLightIdx = (bIsTranslucent) ? SpotLightListTrans.Load(TileOffset) : SpotLightList.Load(TileOffset);
        TileOffset += 4;
		
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        TraceSpotShadow(TLAS, SpotLight, WorldPos, WorldNormal, Gap, MipLevel, Atten, MaxDist);
    }
	
	// saturate the attenuation and output
    Atten = saturate(Atten);
    OutShadowResult[PixelCoord] = float2(MaxDist, Atten);
}

[shader("raygeneration")]
void RTShadowRayGen() 
{
    // the tracing could be half-sized, but now the buffer is always the same resolution as rendering
    // so need to calculate proper pixel coordinate here
	uint2 PixelCoord = DispatchRaysIndex().xy * GResolution.xy * GShadowResolution.zw;
    OutShadowResult[PixelCoord] = 0;
	
	// early return if no lights
	UHBRANCH
	if (!HasLighting())
	{
		return;
	}

    // to UV
	float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;

	// calculate mip level before ray tracing kicks off
    float MipRate = MixedMipTexture.SampleLevel(LinearSampler, ScreenUV, 0).r;
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;

    // trace shadow just once, it will take care opaque/translucent tracing at the same time
	TraceShadow(PixelCoord, ScreenUV, MipRate, MipLevel);
}

[shader("miss")]
void RTShadowMiss(inout UHDefaultPayload Payload)
{
	// do nothing
}