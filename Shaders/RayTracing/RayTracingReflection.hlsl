// RayTracingReflection.hlsl -  Shader for opaque reflection tracing
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
Texture2D SceneMipTexture : register(t8);
Texture2D<uint> SceneDataTexture : register(t9);
Texture2D SmoothSceneNormalTexture : register(t10);

// lighting parameters
StructuredBuffer<UHInstanceLights> InstanceLights : register(t11);
ByteAddressBuffer PointLightList : register(t12);
ByteAddressBuffer SpotLightList : register(t13);
TextureCube EnvCube : register(t14);

// samplers
SamplerState EnvSampler : register(s15);
SamplerState PointClampped : register(s16);
SamplerState LinearClampped : register(s17);

static const float GShadowRayGap = 0.01f;

void ConditionalCalculatePointLight(uint TileIndex, in UHDefaultPayload Payload, UHLightInfo LightInfo
    , float MipLevel, inout float3 Result)
{
    // fetch tiled point light
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightList.Load(TileOffset);
    TileOffset += 4;
    
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
                float HitDist = 0;
                float Atten = 1.0f;
            
                TracePointShadow(TLAS, PointLight, LightInfo.WorldPos, LightInfo.Normal, GShadowRayGap, MipLevel, Atten, HitDist);
                LightInfo.ShadowMask = Atten;
                Result += CalculatePointLight(PointLight, LightInfo, true);
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
                float HitDist = 0;
                float Atten = 1.0f;
            
                TracePointShadow(TLAS, PointLight, LightInfo.WorldPos, LightInfo.Normal, GShadowRayGap, MipLevel, Atten, HitDist);
                LightInfo.ShadowMask = Atten;
                Result += CalculatePointLight(PointLight, LightInfo, true);
            }
        }
    }
}

void ConditionalCalculateSpotLight(uint TileIndex, in UHDefaultPayload Payload, UHLightInfo LightInfo
    , float MipLevel, inout float3 Result)
{
    // fetch tiled spot light
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
                float HitDist = 0;
                float Atten = 1.0f;
            
                TraceSpotShadow(TLAS, SpotLight, LightInfo.WorldPos, LightInfo.Normal, GShadowRayGap, MipLevel, Atten, HitDist);
                LightInfo.ShadowMask = Atten;
                Result += CalculateSpotLight(SpotLight, LightInfo, true);
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
                float HitDist = 0;
                float Atten = 1.0f;
            
                TraceSpotShadow(TLAS, SpotLight, LightInfo.WorldPos, LightInfo.Normal, GShadowRayGap, MipLevel, Atten, HitDist);
                LightInfo.ShadowMask = Atten;
                Result += CalculateSpotLight(SpotLight, LightInfo, true);
            }
        }
    }
}

float4 CalculateReflectionLighting(in UHDefaultPayload Payload
    , float3 HitWorldPos
    , float MipLevel
    , float3 ReflectRay)
{
    // doing the same lighting as object pass except the indirect specular
    float3 Result = 0;
    bool bHitRefraction = (Payload.PayloadData & PAYLOAD_HITREFRACTION) > 0;
    float2 ScreenUV = Payload.HitScreenUV;
    
    Payload.HitMaterialNormal = normalize(Payload.HitMaterialNormal);
    Payload.HitVertexNormal = normalize(Payload.HitVertexNormal);
    
    // light calculation, be sure to normalize normal vector before using it
    UHLightInfo LightInfo;
    LightInfo.Diffuse = Payload.HitDiffuse.rgb;
    LightInfo.Specular = Payload.HitSpecular;
    LightInfo.Normal = Payload.HitMaterialNormal;
    LightInfo.WorldPos = HitWorldPos;
    
    // check whether it's a refraction material
    // if yes, shoot another ray for it
    if (bHitRefraction)
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
    
    // noise setup for color banding
    LightInfo.AttenNoise = GetAttenuationNoise(PixelCoord.xy) * 0.1f;
    LightInfo.SpecularNoise = LightInfo.AttenNoise * lerp(0.5f, 0.02f, LightInfo.Specular.a);
    
    // directional lights, with max number limitation
    if (GNumDirLights > 0)
    {
        for (uint Ldx = 0; Ldx < min(GNumDirLights, GRTMaxDirLight); Ldx++)
        {
            UHDirectionalLight DirLight = UHDirLights[Ldx];
            UHBRANCH
            if (DirLight.bIsEnabled)
            {
                float HitDist = 0;
                float Atten = 1.0f;
            
                TraceDiretionalShadow(TLAS, DirLight, LightInfo.WorldPos, LightInfo.Normal, GShadowRayGap, MipLevel, Atten, HitDist);
                LightInfo.ShadowMask = Atten;
                Result += CalculateDirLight(DirLight, LightInfo, true);
            }
        }
    }
    
    // point lights
    if (GNumPointLights > 0)
    {
        ConditionalCalculatePointLight(TileIndex, Payload, LightInfo, MipLevel, Result);
    }
    
    // spot lights
    if (GNumSpotLights > 0)
    {
        ConditionalCalculateSpotLight(TileIndex, Payload, LightInfo, MipLevel, Result);
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
    bool bHalfQuality = GRTReflectionQuality == 1;
    
    uint2 PixelCoord = DispatchRaysIndex().xy;
    if (bHalfQuality)
    {
        PixelCoord *= 2;
    }
    
    // To UV
    float2 ScreenUV = (PixelCoord + 0.5f) * GResolution.zw;
    float4 OpaqueBump = SceneBuffers[1].SampleLevel(LinearClampped, ScreenUV, 0);
    
    bool bTraceThisPixel = OpaqueBump.a > 0;
    if (!bTraceThisPixel)
    {
        return;
    }
    
    float Smoothness = SceneBuffers[2].SampleLevel(LinearClampped, ScreenUV, 0).a;
    if (Smoothness <= GRTReflectionSmoothCutoff)
    {
        return;
    }
    
    // Now fetch the data used for RT    
    // simulate the mip level based on rendering resolution, carry mipmap count data in hit group if this is not enough
    float MipRate = SceneMipTexture.SampleLevel(LinearClampped, ScreenUV, 0).r;
    float MipLevel = max(0.5f * log2(MipRate * MipRate), 0) * GScreenMipCount + GRTMipBias;

    float SceneDepth = SceneBuffers[3].SampleLevel(PointClampped, ScreenUV, 0).r;
    float3 SceneWorldPos = ComputeWorldPositionFromDeviceZ_UV(ScreenUV, SceneDepth, true);
    
    uint PackedSceneData = SceneDataTexture[PixelCoord];
    
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
        SceneNormal = OpaqueBump.xyz;
        SceneNormal = DecodeNormal(SceneNormal);
    }
    
    float RayGap = lerp(GRTGapMin, GRTGapMax, saturate(MipRate * RT_MIPRATESCALE));
    
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
        float3 HitWorldPos = ReflectRay.Origin + ReflectRay.Direction * Payload.HitT;
        OutResult[PixelCoord] = CalculateReflectionLighting(Payload, HitWorldPos, MipLevel, ReflectRay.Direction);
    }
}

[shader("miss")]
void RTReflectionMiss(inout UHDefaultPayload Payload)
{
	// do nothing, the reflection will just remain black (0,0,0,0)
    // system will blend the result in the object pass based on the alpha
}