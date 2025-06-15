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
Texture2D SmoothReflectVecTexture : register(t13);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t14);
ByteAddressBuffer PointLightListTrans : register(t15);
ByteAddressBuffer SpotLightListTrans : register(t16);
TextureCube EnvCube : register(t17);

// samplers
SamplerState PointClampSampler : register(s18);
SamplerState LinearClampSampler : register(s19);
SamplerState EnvSampler : register(s20);

static const int GMaxDirLight = 2;

void ConditionalCalculatePointLight(uint TileIndex, in UHDefaultPayload Payload, UHLightInfo LightInfo, inout float3 Result)
{
    // fetch tiled point light
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(TileOffset);
    TileOffset += 4;
    
    UHBRANCH
    if (Payload.IsInsideScreen && PointLightCount > 0)
    {
        for (uint Ldx = 0; Ldx < PointLightCount; Ldx++)
        {
            uint PointLightIdx = PointLightListTrans.Load(TileOffset);
            TileOffset += 4;
       
            UHPointLight PointLight = UHPointLights[PointLightIdx];
            Result += CalculatePointLight(PointLight, LightInfo);
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
            Result += CalculatePointLight(PointLight, LightInfo);
        }
    }
}

void ConditionalCalculateSpotLight(uint TileIndex, in UHDefaultPayload Payload, UHLightInfo LightInfo, inout float3 Result)
{
    // fetch tiled spot light
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
            Result += CalculateSpotLight(SpotLight, LightInfo);
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
            Result += CalculateSpotLight(SpotLight, LightInfo);
        }
    }
}

float AccumulateShadowAtten(float InAtten, float NdotL, in UHDefaultPayload ReflectShadowPayload)
{
    if (ReflectShadowPayload.IsHit())
    {
        InAtten = lerp(InAtten + NdotL, InAtten, ReflectShadowPayload.HitAlpha);
    }
    else
    {
        InAtten += NdotL;
    }
    
    return InAtten;
}

float TraceShadowInReflection(float3 HitWorldPos, uint TileIndex, in UHDefaultPayload Payload, float MipLevel)
{
    float Atten = 0.0f;
    float3 HitWorldNormal = Payload.HitWorldNormal;
    
    RayDesc ShadowRay = (RayDesc)0;
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
            if (TraceDiretionalShadow(TLAS, DirLight, HitWorldPos, HitWorldNormal, Gap, MipLevel, Atten, Dummy))
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
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitWorldNormal, Gap, MipLevel, Atten, Dummy);
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
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitWorldNormal, Gap, MipLevel, Atten, Dummy);
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
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitWorldNormal, Gap, MipLevel, Atten, Dummy);
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
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitWorldNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    
    return saturate(Atten);
}

float4 CalculateReflectionLighting(in UHDefaultPayload Payload
    , float3 HitWorldPos
    , float MipLevel
    , float3 ReflectRay)
{
    // doing the same lighting as object pass except the indirect specular
    precise float3 Result = 0;
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
        Payload.HitWorldNormal = lerp(Payload.HitWorldNormal, Payload.HitWorldNormalTrans, TransOpacity);
        Payload.HitEmissive.rgb = lerp(Payload.HitEmissive.rgb, Payload.HitEmissiveTrans.rgb, TransOpacity);
    }
    Payload.HitNormal = normalize(Payload.HitNormal);
    Payload.HitWorldNormal = normalize(Payload.HitWorldNormal);
    
    // light calculation, be sure to normalize normal vector before using it
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Payload.HitDiffuse.rgb;
    LightInfo.Specular = Payload.HitSpecular;
    LightInfo.Normal = Payload.HitNormal;
    LightInfo.WorldPos = bHitTranslucent ? Payload.HitWorldPosTrans : HitWorldPos;
    
    // check whether it's a refraction material
    // if yes, shoot another ray for it
    if (bHitTranslucent && bHitRefraction)
    {
        RayDesc RefractRay = (RayDesc) 0;
        
        // modify the ray direction related to camera
        float3 EyeToHitPos = LightInfo.WorldPos - GCameraPos;
        EyeToHitPos += float3(Payload.HitRefractOffset * 0.5f, 0.0f);
        ReflectRay = normalize(EyeToHitPos);
        
        RefractRay.Origin = GCameraPos;
        RefractRay.Direction = ReflectRay;
        RefractRay.TMin = 0.0f;
        RefractRay.TMax = GRTReflectionRayTMax;

        UHDefaultPayload RefractPayload = (UHDefaultPayload) 0;
        RefractPayload.MipLevel = MipLevel;
        RefractPayload.PayloadData |= PAYLOAD_ISREFLECTION;
            
        TraceRay(TLAS, RAY_FLAG_CULL_NON_OPAQUE, 0xff, 0, 0, 0, RefractRay, RefractPayload);
            
        // proceed to opaque lighting if hit
        if (RefractPayload.IsHit())
        {
            // set payload to the newly hit one
            Payload = RefractPayload;
            ScreenUV = Payload.HitScreenUV;
            LightInfo.Diffuse = Payload.HitDiffuse.rgb;
            LightInfo.Specular = Payload.HitSpecular;
            LightInfo.Normal = Payload.HitNormal;
            LightInfo.WorldPos = RefractRay.Origin + RefractRay.Direction * Payload.HitT;
            LightInfo.WorldPos += Payload.HitWorldNormal * 0.1f;
        }
        else
        {
            // consider the light is absorbed if the refract ray missed
            return 0;
        }
    }
    
    // for point lights and spot lights, fetch from tile-based light if it's inside screen
    // otherwise, fetch from the closest lights to current camera for now
    uint2 PixelCoord = uint2(ScreenUV * GResolution.xy);
    uint TileX = CoordToTileX(PixelCoord.x);
    uint TileY = CoordToTileY(PixelCoord.y);
    uint TileIndex = TileX + TileY * GLightTileCountX;
    
    // trace shadow in reflection
    LightInfo.ShadowMask = TraceShadowInReflection(LightInfo.WorldPos, TileIndex, Payload, MipLevel);
    LightInfo.AttenNoise = GetAttenuationNoise(PixelCoord.xy) * 0.1f;
    LightInfo.SpecularNoise = LightInfo.AttenNoise * lerp(0.5f, 0.02f, LightInfo.Specular.a);
    
    // directional lights, with max number limitation
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GMaxDirLight); Ldx++)
        {
            Result += CalculateDirLight(UHDirLights[Ldx], LightInfo);
        }
    }
    
    // point lights
    if (GNumPointLights > 0)
    {
        ConditionalCalculatePointLight(TileIndex, Payload, LightInfo, Result);
    }
    
    // spot lights
    if (GNumSpotLights > 0)
    {
        ConditionalCalculateSpotLight(TileIndex, Payload, LightInfo, Result);
    }
    
    // indirect light and emissive
    {
        float Occlusion = Payload.HitDiffuse.a;
        Result += ShadeSH9(LightInfo.Diffuse.rgb, float4(LightInfo.Normal, 1.0f), Occlusion);
        Result += Payload.HitEmissive;
        
        // still need to sample sky reflection here, in case the ray is hitting a material that has high metallic
        // which makes it black. Or the back side of an object (low lighting)
        float SpecFade = LightInfo.Specular.a * LightInfo.Specular.a;
        float SpecMip = (1.0f - SpecFade) * GEnvCubeMipMapCount;
        
        // here needs the previous ray dir for sky lookup as it might be from multiple bounce ray
        // can't evaluate from camera pos
        float3 EyeVector = Payload.RayDir;
        float3 R = reflect(EyeVector, LightInfo.Normal);
        
        // fresnel calculation
        float NdotV = abs(dot(LightInfo.Normal, -EyeVector));
        // Payload.PackedData0.a for fresnel factor from material
        float3 Fresnel = SchlickFresnel(LightInfo.Specular.rgb, lerp(0, NdotV, Payload.PackedData0.a));
     
        Result += EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * GAmbientSky * SpecFade * GFinalReflectionStrength
            // occlusion is stored in diffuse.a
            * Occlusion * Fresnel;
    }
    
    return float4(Result, Payload.HitAlpha);
}

[shader("raygeneration")]
void RTReflectionRayGen()
{
    bool bHalfPixel = GRTReflectionQuality == 1;
    bool bQuarterPixel = GRTReflectionQuality == 2;
    
    uint2 PixelCoord = DispatchRaysIndex().xy;
    if (bQuarterPixel)
    {
        PixelCoord *= 2;
    }
    
    if (bHalfPixel && (PixelCoord.x & 1) != 0)
    {
        // for half tracing, skip it if the pattern isn't fit
        return;
    }
    
    // To UV
    float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;
    float4 OpaqueBump = SceneBuffers[1][PixelCoord];
    float4 TranslucentBump = TranslucentBumpTexture[PixelCoord];
    
    bool bHasOpaqueInfo = OpaqueBump.a == UH_OPAQUE_MASK;
    bool bHasTranslucentInfo = TranslucentBump.a == UH_TRANSLUCENT_MASK;
    bool bTraceThisPixel = bHasOpaqueInfo || bHasTranslucentInfo;
    if (!bTraceThisPixel)
    {
        return;
    }
    
    float Smoothness = bHasTranslucentInfo ? TranslucentSmoothTexture[PixelCoord].r
        : SceneBuffers[2][PixelCoord].a;
    if (Smoothness <= GRTReflectionSmoothCutoff)
    {
        return;
    }
    
    // Now fetch the data used for RT
    float MipRate = MixedMipTexture.SampleLevel(LinearClampSampler, ScreenUV, 0).r;
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;
    
    float3 VertexNormal = DecodeNormal(MixedVertexNormalTexture.SampleLevel(PointClampSampler, ScreenUV, 0).xyz);
    // Select from translucent or opaque bump
    float3 SceneNormal = bHasTranslucentInfo ? TranslucentBump.xyz : OpaqueBump.xyz;
    SceneNormal = DecodeNormal(SceneNormal);

    float SceneDepth = MixedDepthTexture.SampleLevel(PointClampSampler, ScreenUV, 0).r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    
    // Calculate reflect ray
    // in real world, there aren't actually a "perfect" surface on most objects except mirrors
    // so giving it a small distortion if it's using vertex normal
    bool bUseVertexNormal = length(VertexNormal - SceneNormal) < 0.0001f;
    
    float3 EyeVector = SceneWorldPos - GCameraPos;
    if (bUseVertexNormal)
    {
        float EyeLength = length(EyeVector);
        float OffsetAlpha = saturate(EyeLength / 5.0f);
        
        float YOffset = SineApprox(PixelCoord.y * GResolution.w * UH_PI * 0.5f);
        YOffset *= lerp(0.0f, 0.5f, OffsetAlpha);
        
        EyeVector += float3(0, YOffset, 0);
    }
    EyeVector = normalize(EyeVector);
    
    // fetch refined eye vector for reflection to reduce noise for bump normal
    // or reflect vertex normal ray
    float3 ReflectedRay = bUseVertexNormal || !(GSystemRenderFeature & UH_RTREFLECTION_DENOISE) ? reflect(EyeVector, SceneNormal)
        : SmoothReflectVecTexture.SampleLevel(LinearClampSampler, ScreenUV, 0).xyz;
    float RayGap = lerp(0.01f, 0.05f, saturate(MipRate * RT_MIPRATESCALE));
    
    RayDesc ReflectRay = (RayDesc) 0;
    ReflectRay.Origin = SceneWorldPos + VertexNormal * RayGap;
    ReflectRay.Direction = ReflectedRay;
    
    ReflectRay.TMin = RayGap;
    ReflectRay.TMax = GRTReflectionRayTMax;

    UHDefaultPayload Payload = (UHDefaultPayload) 0;
    Payload.MipLevel = MipLevel;
    Payload.PayloadData |= PAYLOAD_ISREFLECTION;
    // init CurrentRecursion as 1
    Payload.CurrentRecursion = 1;

    TraceRay(TLAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, ReflectRay, Payload);
    
    if (Payload.IsHit())
    {
        float3 HitWorldPos = Payload.PackedData0.xyz;
        OutResult[PixelCoord] = CalculateReflectionLighting(Payload, HitWorldPos, MipLevel, ReflectRay.Direction);
    }
}

[shader("miss")]
void RTReflectionMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the reflection will just remain black (0,0,0,0)
    // system will blend the result in the object pass based on the alpha
}