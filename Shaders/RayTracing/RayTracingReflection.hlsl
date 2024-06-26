// RT reflection implementation in UHE
#define UHGBUFFER_BIND t3
#define UHDIRLIGHT_BIND t4
#define UHPOINTLIGHT_BIND t5
#define UHSPOTLIGHT_BIND t6
#define SH9_BIND t7
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "../UHMaterialCommon.hlsli"
#include "../UHSphericalHamonricCommon.hlsli"


RaytracingAccelerationStructure TLAS : register(t1);
// Output result, alpha channel will be the flag to tell whether RT reflection is hit (valid)
// If not, sample the skybox instead. The format is float 16-bit.
RWTexture2D<float4> OutResult : register(u2);

// Buffers needed. The workflow will shoot ray from buffer instead of shooting a 'search ray' to reduce the number of rays per frame.
Texture2D MixedMipTexture : register(t8);
Texture2D MixedDepthTexture : register(t9);
Texture2D MixedVertexNormalTexture : register(t10);
Texture2D TranslucentBumpTexture : register(t11);
Texture2D TranslucentSmoothTexture : register(t12);

// lighting parameters
Texture2D ScreenShadowTexture : register(t13);
ByteAddressBuffer PointLightListTrans : register(t14);
ByteAddressBuffer SpotLightListTrans : register(t15);

// scene textures for refraction
Texture2D SceneTexture : register(t16);
Texture2D BlurredSceneTexture : register(t17);

// samplers
SamplerState PointClampSampler : register(s18);
SamplerState LinearClampSampler : register(s19);

static const int GMaxDirLight = 4;
static const int GMaxPointLight = 8;
static const int GMaxSpotLight = 8;

void ConditionalCalculatePointLight(uint TileIndex, float AttenNoise, in UHDefaultPayload Payload, UHLightInfo LightInfo, inout float3 Result)
{
    // fetch tiled point light
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(TileOffset);
    TileOffset += 4;
    
    float3 WorldPos = LightInfo.WorldPos;
    float ShadowMask = LightInfo.ShadowMask;
    
    float3 LightToWorld;
    float LightAtten;
    
    UHBRANCH
    if (Payload.IsInsideScreen)
    {    
        for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
        {
            uint PointLightIdx = PointLightListTrans.Load(TileOffset);
            TileOffset += 4;
       
            UHPointLight PointLight = UHPointLights[PointLightIdx];
            UHBRANCH
            if (!PointLight.bIsEnabled)
            {
                continue;
            }
            LightInfo.LightColor = PointLight.Color.rgb;
		
            LightToWorld = WorldPos - PointLight.Position;
            LightInfo.LightDir = normalize(LightToWorld);
		
		    // square distance attenuation
            LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + AttenNoise);
            LightAtten *= LightAtten;
            LightInfo.ShadowMask = LightAtten * ShadowMask;
		
            Result += LightBRDF(LightInfo);
        }
    }
    else
    {
        for (uint Ldx = 0; Ldx < min(GNumPointLights, GMaxPointLight); Ldx++)
        {
            UHPointLight PointLight = UHPointLights[Ldx];
            UHBRANCH
            if (!PointLight.bIsEnabled)
            {
                continue;
            }
            LightInfo.LightColor = PointLight.Color.rgb;
		
            LightToWorld = WorldPos - PointLight.Position;
            LightInfo.LightDir = normalize(LightToWorld);
		
		    // square distance attenuation
            LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + AttenNoise);
            LightAtten *= LightAtten;
            LightInfo.ShadowMask = LightAtten * ShadowMask;
		
            Result += LightBRDF(LightInfo);
        }
    }
}

void ConditionalCalculateSpotLight(uint TileIndex, float AttenNoise, in UHDefaultPayload Payload, UHLightInfo LightInfo, inout float3 Result)
{
    // fetch tiled spot light
    uint TileOffset = GetSpotLightOffset(TileIndex);
    uint SpotLightCount = SpotLightListTrans.Load(TileOffset);
    TileOffset += 4;
    
    float3 WorldPos = LightInfo.WorldPos;
    float ShadowMask = LightInfo.ShadowMask;
    
    float3 LightToWorld;
    float LightAtten;
    
    UHBRANCH
    if (Payload.IsInsideScreen)
    {        
        for (uint Ldx = 0; Ldx < SpotLightCount; Ldx++)
        {
            uint SpotLightIdx = SpotLightListTrans.Load(TileOffset);
            TileOffset += 4;
        
            UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
            UHBRANCH
            if (!SpotLight.bIsEnabled)
            {
                continue;
            }
        
            LightInfo.LightColor = SpotLight.Color.rgb;
            LightInfo.LightDir = SpotLight.Dir;
            LightToWorld = WorldPos - SpotLight.Position;
        
            // squared distance attenuation
            LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + AttenNoise);
            LightAtten *= LightAtten;
        
            // squared spot angle attenuation
            float Rho = dot(SpotLight.Dir, normalize(LightToWorld));
            float SpotFactor = (Rho - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
            SpotFactor = saturate(SpotFactor);
        
            LightAtten *= SpotFactor * SpotFactor;
            LightInfo.ShadowMask = LightAtten * ShadowMask;
		
            Result += LightBRDF(LightInfo);
        }
    }
    else
    {
        for (uint Ldx = 0; Ldx < min(GNumSpotLights, GMaxSpotLight); Ldx++)
        {
            UHSpotLight SpotLight = UHSpotLights[Ldx];
            UHBRANCH
            if (!SpotLight.bIsEnabled)
            {
                continue;
            }
        
            LightInfo.LightColor = SpotLight.Color.rgb;
            LightInfo.LightDir = SpotLight.Dir;
            LightToWorld = WorldPos - SpotLight.Position;
        
            // squared distance attenuation
            LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + AttenNoise);
            LightAtten *= LightAtten;
        
            // squared spot angle attenuation
            float Rho = dot(SpotLight.Dir, normalize(LightToWorld));
            float SpotFactor = (Rho - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
            SpotFactor = saturate(SpotFactor);
        
            LightAtten *= SpotFactor * SpotFactor;
            LightInfo.ShadowMask = LightAtten * ShadowMask;
		
            Result += LightBRDF(LightInfo);
        }
    }
}

float4 CalculateReflectionLighting(in UHDefaultPayload Payload, float3 HitWorldPos, float3 SceneWorldPos)
{
    // doing the same lighting as object pass except the indirect specular
    float3 Result = 0;
    bool bHitTranslucent = (Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) > 0;
    bool bHitRefraction = (Payload.PayloadData & PAYLOAD_HITREFRACTION) > 0;
    float2 ScreenUV = bHitTranslucent ? Payload.HitScreenUVTrans : Payload.HitScreenUV;
    
    // blend the hit info with translucent opactiy when necessary
    UHDefaultPayload OriginPayload = Payload;
    if (bHitTranslucent)
    {
        float TransOpacity = Payload.HitEmissiveTrans.a;
        Payload.HitDiffuse = lerp(Payload.HitDiffuse, Payload.HitDiffuseTrans, TransOpacity);
        Payload.HitSpecular = lerp(Payload.HitSpecular, Payload.HitSpecularTrans, TransOpacity);
        Payload.HitNormal = lerp(Payload.HitNormal, Payload.HitNormalTrans, TransOpacity);
        Payload.HitEmissive.rgb = lerp(Payload.HitEmissive.rgb, Payload.HitEmissiveTrans.rgb, TransOpacity);
        Payload.HitAlpha = TransOpacity;
    }
    
    // light calculation, be sure to normalize normal vector before using it
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Payload.HitDiffuse.rgb;
    LightInfo.Specular = Payload.HitSpecular;
    LightInfo.Normal = normalize(Payload.HitNormal);
    LightInfo.WorldPos = bHitTranslucent ? Payload.HitWorldPosTrans : HitWorldPos;
    
    // check whether it's a refraction material
    float3 RefractionResult = 0;
    if (bHitTranslucent && bHitRefraction)
    {
        float3 EyeDir = LightInfo.WorldPos - GCameraPos;
        float EyeLength = length(EyeDir);
        EyeDir = normalize(EyeDir);
        
        float3 RefractEyeVec = refract(EyeDir, LightInfo.Normal, Payload.HitRefraction);
        float2 RefractOffset = (RefractEyeVec.xy - EyeDir.xy) / EyeLength;
        float2 RefractUV = ScreenUV + Payload.HitRefractScale * RefractOffset;
    
        // @TODO: Refraction is only for the pixel inside the screen, improve this in the future
        if (IsUVInsideScreen(RefractUV))
        {
            float3 SceneColor = SceneTexture.SampleLevel(LinearClampSampler, RefractUV, 0).rgb;
            float3 BlurredSceneColor = BlurredSceneTexture.SampleLevel(LinearClampSampler, RefractUV, 0).rgb;
    
            // Use the scene color for refraction, the original BaseColor will be used as tint color instead
            // Roughness will be used as a factor for lerp clear/blurred scene
            float SmoothnessSquare = Payload.HitSpecular.a * Payload.HitSpecular.a;
            float3 RefractionColor = lerp(BlurredSceneColor, SceneColor, SmoothnessSquare * SmoothnessSquare);
            RefractionResult = RefractionColor * LightInfo.Diffuse + Payload.HitEmissive.rgb;
            
            // final lerp with the scene color, which is different from the calculation in TranslucentPixelShader
            // I'm lack of alpha blend output here and need to simulate it myself
            RefractionResult = lerp(SceneColor, RefractionResult, Payload.HitAlpha);
    
            // Early return and do not proceed to lighting, refraction should rely on the lit opaque scene.
            return float4(RefractionResult, 1.0f);
        }
        else
        {
            // fallback to opaque lighting
            Payload = OriginPayload;
            ScreenUV = Payload.HitScreenUV;
            LightInfo.Diffuse = Payload.HitDiffuse.rgb;
            LightInfo.Specular = Payload.HitSpecular;
            LightInfo.Normal = normalize(Payload.HitNormal);
            LightInfo.WorldPos = HitWorldPos;
        }
    }
    
    // shadows, reuse the screen shadow when possible
    // @TODO: Fallback to other shadow method for the hit position outside of screen
    float ShadowMask = (Payload.IsInsideScreen) ? ScreenShadowTexture.SampleLevel(LinearClampSampler, ScreenUV, 0).r : 1.0f;
    LightInfo.ShadowMask = ShadowMask;
    
    // directional lights, with max number limitation
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GMaxDirLight); Ldx++)
        {
            UHBRANCH
            if (!UHDirLights[Ldx].bIsEnabled)
            {
                continue;
            }
                    
            LightInfo.LightColor = UHDirLights[Ldx].Color.rgb;
            LightInfo.LightDir = UHDirLights[Ldx].Dir;
            
            Result += LightBRDF(LightInfo);
        }
    }
    
    // for point lights and spot lights, fetch from tile-based light if it's inside screen
    // otherwise, fetch from the closest lights to current camera for now, this can be improved by 3D culling instead in the future
    uint2 PixelCoord = uint2(ScreenUV * GResolution.xy);
    uint TileX = CoordToTileX(PixelCoord.x);
    uint TileY = CoordToTileY(PixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    float AttenNoise = GetAttenuationNoise(PixelCoord.xy);
    
    // point lights
    {
        ConditionalCalculatePointLight(TileIndex, AttenNoise, Payload, LightInfo, Result);
    }
    
    // spot lights
    {
        ConditionalCalculateSpotLight(TileIndex, AttenNoise, Payload, LightInfo, Result);
    }
    
    // indirect light and emissive
    {
        Result += ShadeSH9(Payload.HitDiffuse.rgb, float4(Payload.HitNormal, 1.0f), Payload.HitDiffuse.a);
        Result += Payload.HitEmissive;
    }
    
    return float4(Result, Payload.HitAlpha);
}

[shader("raygeneration")]
void RTReflectionRayGen()
{
    bool bHalfPixel = GRTReflectionQuality == 1;
    bool bQuarterPixel = GRTReflectionQuality == 2;
    
    uint2 PixelCoord = DispatchRaysIndex().xy;
    int Dx[3] = { 1, 0, 1 };
    int Dy[3] = { 0, 1, 1 };
    if (bQuarterPixel)
    {
        // for quarter tracing, adjust the pixel coord as cpp side will dispatch at half resolution
        // also needs to clear its neighbor pixels
        PixelCoord *= 2;
        for (int I = 0; I < 3; I++)
        {
            int2 Pos = PixelCoord + int2(Dx[I], Dy[I]);
            Pos = min(Pos, GResolution.xy - 1);
            OutResult[Pos] = 0.0f;
        }
    }
    
    OutResult[PixelCoord] = 0.0f;
    if (bHalfPixel && (PixelCoord.x & 1) != 0)
    {
        // for half tracing, skip it if the pattern isn't fit
        return;
    }
    
    // To UV
    float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;
    float4 OpaqueBump = SceneBuffers[1].SampleLevel(PointClampSampler, ScreenUV, 0);
    float4 TranslucentBump = TranslucentBumpTexture.SampleLevel(PointClampSampler, ScreenUV, 0);
    
    bool bHasOpaqueInfo = OpaqueBump.a == UH_OPAQUE_MASK;
    bool bHasTranslucentInfo = TranslucentBump.a == UH_TRANSLUCENT_MASK;
    bool bTraceThisPixel = bHasOpaqueInfo || bHasTranslucentInfo;
    
    // early return if it has no info  
    UHBRANCH
    if (!bTraceThisPixel)
    {
        return;
    }
    
    float Smoothness = bHasTranslucentInfo ? TranslucentSmoothTexture.SampleLevel(LinearClampSampler, ScreenUV, 0).r
        : SceneBuffers[2].SampleLevel(LinearClampSampler, ScreenUV, 0).a;
    
    // early return if it has low smoothness 
    UHBRANCH
    if (Smoothness <= GRTReflectionSmoothCutoff)
    {
        return;
    }
    
    // Now fetch the data used for RT
    float MipRate = MixedMipTexture.SampleLevel(LinearClampSampler, ScreenUV, 0).r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0);
    
    float3 VertexNormal = DecodeNormal(MixedVertexNormalTexture.SampleLevel(PointClampSampler, ScreenUV, 0).xyz);
    // Select from translucent or opaque bump
    float3 BumpNormal = bHasTranslucentInfo ? TranslucentBump.xyz : OpaqueBump.xyz;
    BumpNormal = DecodeNormal(BumpNormal);

    float SceneDepth = MixedDepthTexture.SampleLevel(PointClampSampler, ScreenUV, 0).r;
    float3 WorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth);
    
    // Calculate reflect ray
    float3 EyeVector = normalize(WorldPos - GCameraPos);
    float3 ReflectedRay = reflect(EyeVector, BumpNormal);
    float RayGap = lerp(0.01f, 0.05f, saturate(MipRate * RT_MIPRATESCALE));
    
    RayDesc ReflectRay = (RayDesc) 0;
    ReflectRay.Origin = WorldPos + VertexNormal * RayGap;
    ReflectRay.Direction = ReflectedRay;

    ReflectRay.TMin = RayGap;
    ReflectRay.TMax = GRTReflectionRayTMax;

    UHDefaultPayload Payload = (UHDefaultPayload) 0;
    Payload.MipLevel = MipLevel;
    Payload.PayloadData |= PAYLOAD_ISREFLECTION;

    TraceRay(TLAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ReflectRay, Payload);
    
    if (Payload.IsHit())
    {
        float3 HitWorldPos = ReflectRay.Origin + ReflectRay.Direction * Payload.HitT;
        float3 EyeVec = HitWorldPos - GCameraPos;
        OutResult[PixelCoord] = CalculateReflectionLighting(Payload, HitWorldPos, WorldPos);
        
        if (bHalfPixel)
        {
            // for half pixel tracing, fill the neighborhood pixels that are not traced this frame
            OutResult[min(PixelCoord + uint2(1, 0), GResolution.xy - 1)] = OutResult[PixelCoord];
        }
        else if (bQuarterPixel)
        {
            // for quarter tracing, fill the neighborhood pixels with a 2x2 box
            for (int I = 0; I < 3; I++)
            {
                int2 Pos = PixelCoord + int2(Dx[I], Dy[I]);
                Pos = min(Pos, GResolution.xy - 1);
                OutResult[Pos] = OutResult[PixelCoord];
            }
        }
    }
}

[shader("miss")]
void RTReflectionMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the reflection will just remain black (0,0,0,0)
    // system will blend the result in the object pass based on the alpha
}