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
Texture2D<uint> MixedDataTexture : register(t9);
Texture2D MixedDepthTexture : register(t10);
Texture2D TranslucentBumpTexture : register(t11);
Texture2D TranslucentSmoothTexture : register(t12);
Texture2D SmoothSceneNormalTexture : register(t13);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t14);
ByteAddressBuffer PointLightListTrans : register(t15);
ByteAddressBuffer SpotLightListTrans : register(t16);
TextureCube EnvCube : register(t17);

// samplers
SamplerState EnvSampler : register(s18);
SamplerState LinearClampped : register(s19);

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

float TraceReflectionShadow(float3 HitWorldPos, uint TileIndex, in UHDefaultPayload Payload, float MipLevel)
{
    float Atten = 0.0f;
    float3 HitVertexNormal = Payload.HitVertexNormal;
    
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
            if (TraceDiretionalShadow(TLAS, DirLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy))
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
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
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
            TracePointShadow(TLAS, PointLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
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
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
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
            TraceSpotShadow(TLAS, SpotLight, HitWorldPos, HitVertexNormal, Gap, MipLevel, Atten, Dummy);
        }
    }
    
    return saturate(Atten);
}

float4 CalculateReflectionLighting(in UHDefaultPayload Payload
    , float MipLevel
    , float3 ReflectRay)
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
        Payload.HitMaterialNormal = lerp(Payload.HitMaterialNormal, Payload.HitMaterialNormalTrans, TransOpacity);
        Payload.HitVertexNormal = lerp(Payload.HitVertexNormal, Payload.HitVertexNormalTrans, TransOpacity);
        Payload.HitEmissive.rgb = lerp(Payload.HitEmissive.rgb, Payload.HitEmissiveTrans.rgb, TransOpacity);
    }
    Payload.HitMaterialNormal = normalize(Payload.HitMaterialNormal);
    Payload.HitVertexNormal = normalize(Payload.HitVertexNormal);
    
    // light calculation, be sure to normalize normal vector before using it
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Payload.HitDiffuse.rgb;
    LightInfo.Specular = Payload.HitSpecular;
    LightInfo.Normal = Payload.HitMaterialNormal;
    LightInfo.WorldPos = bHitTranslucent ? Payload.HitWorldPosTrans : Payload.HitWorldPos;
    
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
            LightInfo.Normal = Payload.HitMaterialNormal;
            LightInfo.WorldPos = RefractRay.Origin + RefractRay.Direction * Payload.HitT;
            LightInfo.WorldPos += Payload.HitVertexNormal * 0.1f;
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
    LightInfo.ShadowMask = TraceReflectionShadow(LightInfo.WorldPos, TileIndex, Payload, MipLevel);
    LightInfo.AttenNoise = GetAttenuationNoise(PixelCoord.xy) * 0.1f;
    LightInfo.SpecularNoise = LightInfo.AttenNoise * lerp(0.5f, 0.02f, LightInfo.Specular.a);
    
    // directional lights, with max number limitation
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
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
        float3 Fresnel = SchlickFresnel(LightInfo.Specular.rgb, lerp(0, NdotV, Payload.FresnelFactor));
     
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
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipRate = MixedMipTexture[PixelCoord].r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;

    float SceneDepth = MixedDepthTexture[PixelCoord].r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    
    uint PackedSceneData = MixedDataTexture[PixelCoord];
    
    // Calculate reflect ray
    // in real world, there aren't actually a "perfect" surface on most objects
    // so giving it a small distortion
    float3 EyeVector = SceneWorldPos - GCameraPos;
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
    bool bDenoise = (GSystemRenderFeature & UH_USE_SMOOTH_NORMAL_RAYTRACING);
    bool bHasBumpThisPixel = (PackedSceneData.r & UH_HAS_BUMP);
    
    float3 SceneNormal = 0;
    UHBRANCH
    if (bDenoise && bHasBumpThisPixel)
    {
        SceneNormal = SmoothSceneNormalTexture.SampleLevel(LinearClampped, ScreenUV, 0).xyz;
    }
    else
    {
        SceneNormal = bHasTranslucentInfo ? TranslucentBump.xyz : OpaqueBump.xyz;
        SceneNormal = DecodeNormal(SceneNormal);
    }
    
    float RayGap = lerp(0.01f, 0.05f, saturate(MipRate * RT_MIPRATESCALE));
    
    RayDesc ReflectRay = (RayDesc) 0;
    ReflectRay.Origin = SceneWorldPos + SceneNormal * RayGap;
    ReflectRay.Direction = reflect(EyeVector, SceneNormal);
    
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
        OutResult[PixelCoord] = CalculateReflectionLighting(Payload, MipLevel, ReflectRay.Direction);
    }
}

[shader("miss")]
void RTReflectionMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the reflection will just remain black (0,0,0,0)
    // system will blend the result in the object pass based on the alpha
}