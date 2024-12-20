#ifndef UHLIGHTCOMMON_H
#define UHLIGHTCOMMON_H

#define UHLIGHTCULLING_TILEX 16
#define UHLIGHTCULLING_TILEY 16
#define UHLIGHTCULLING_UPSCALE 2

#include "UHInputs.hlsli"
#ifndef UHDIRLIGHT_BIND
#define UHDIRLIGHT_BIND t3
#endif

#ifndef UHPOINTLIGHT_BIND
#define UHPOINTLIGHT_BIND t4
#endif

#ifndef UHSPOTLIGHT_BIND
#define UHSPOTLIGHT_BIND t5
#endif

struct UHDirectionalLight
{
	// color.a for intensity, but it's already applied to color
    float4 Color;
    float3 Dir;
    bool bIsEnabled;
};
StructuredBuffer<UHDirectionalLight> UHDirLights : register(UHDIRLIGHT_BIND);

struct UHPointLight
{
	// color.a for intensity, but it's already applied to color
    float4 Color;
    float Radius;
    float3 Position;
    bool bIsEnabled;
};
StructuredBuffer<UHPointLight> UHPointLights : register(UHPOINTLIGHT_BIND);

struct UHSpotLight
{
	// world to light matrix, so the objects can transform to spot light space
    float4x4 WorldToLight;
	// color.a for intensity, but it's already applied to color
    float4 Color;
    float3 Dir;
    float Radius;
    float Angle;
    float3 Position;
    float InnerAngle;
    bool bIsEnabled;
};
StructuredBuffer<UHSpotLight> UHSpotLights : register(UHSPOTLIGHT_BIND);

struct UHLightInfo
{
    float3 LightColor;
    float3 LightDir;
    float3 Diffuse;
    float4 Specular;
    float3 Normal;
    float3 WorldPos;
    float ShadowMask;
    float AttenNoise;
    float SpecularNoise;
};

float3 SchlickFresnel(float3 R0, float LdotH)
{
    float CosIncidentAngle = LdotH;

    // pow5 F0 is used
    float F0 = 1.0f - CosIncidentAngle;
    float3 ReflectPercent = R0 + (1.0f - R0) * (F0 * F0 * F0 * F0 * F0);

    return ReflectPercent;
}

// blinn phong
float BlinnPhong(float M, float NdotH)
{
    float M4 = M * M * M * M;
    M4 *= 256.0f;
    M4 = max(M4, 0.00001f);
    return (M4 + 4.0f) * pow(NdotH, M4) / 4.0f;
}

float3 LightBRDF(UHLightInfo LightInfo)
{
    float3 LightColor = LightInfo.LightColor * LightInfo.ShadowMask;
    float3 LightDir = -LightInfo.LightDir;
    float3 ViewDir = -normalize(LightInfo.WorldPos - GCameraPos);

    // Diffuse = N dot L
    float NdotL = saturate(dot(LightInfo.Normal, LightDir));
    float3 LightDiffuse = LightColor * NdotL;

    // Specular = SchlickFresnel * BlinnPhong * NdotL
    // safe normalize half dir
    float3 HalfDir = (ViewDir + LightDir) / (length(ViewDir + LightDir) + UH_FLOAT_EPSILON);
    float LdotH = saturate(dot(LightDir, HalfDir));
    
    // reduce color banding in specular highlight by apply a tiny noise to normal
    float NdotH = saturate(dot(LightInfo.Normal + LightInfo.SpecularNoise, HalfDir));

    float Smoothness = LightInfo.Specular.a;
    float3 LightSpecular = LightColor * SchlickFresnel(LightInfo.Specular.rgb, LdotH)
        * BlinnPhong(Smoothness, NdotH) * NdotL;

    return LightInfo.Diffuse * LightDiffuse + LightSpecular;
}

uint GetPointLightOffset(uint InIndex)
{
    return InIndex * (GMaxPointLightPerTile * 4 + 4);
}

uint GetSpotLightOffset(uint InIndex)
{
    return InIndex * (GMaxSpotLightPerTile * 4 + 4);
}

bool HasLighting()
{
    return GNumDirLights > 0 || GNumPointLights > 0 || GNumSpotLights > 0;
}

uint CoordToTileX(uint InX)
{
    return InX / UHLIGHTCULLING_TILEX / UHLIGHTCULLING_UPSCALE;
}

uint CoordToTileY(uint InY)
{
    return InY / UHLIGHTCULLING_TILEY / UHLIGHTCULLING_UPSCALE;
}

float TileToCoordX(uint InX)
{
    return InX * UHLIGHTCULLING_TILEX;
}

float TileToCoordY(uint InY)
{
    return InY * UHLIGHTCULLING_TILEY;
}

float3 CalculateDirLight(UHDirectionalLight DirLight, UHLightInfo LightInfo)
{
    UHBRANCH
    if (!DirLight.bIsEnabled)
    {
        return 0;
    }
        
    LightInfo.LightColor = DirLight.Color.rgb;
    LightInfo.LightDir = DirLight.Dir;
    return LightBRDF(LightInfo);
}

float3 CalculatePointLight(UHPointLight PointLight, UHLightInfo LightInfo)
{
    UHBRANCH
    if (!PointLight.bIsEnabled)
    {
        return 0;
    }
    LightInfo.LightColor = PointLight.Color.rgb;
		
    float3 LightToWorld = LightInfo.WorldPos - PointLight.Position;
    LightInfo.LightDir = normalize(LightToWorld);
		
	// square distance attenuation
    float LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + LightInfo.AttenNoise);
    LightAtten *= LightAtten;
    LightInfo.ShadowMask = LightAtten * LightInfo.ShadowMask;
		
    return LightBRDF(LightInfo);
}

float3 CalculateSpotLight(UHSpotLight SpotLight, UHLightInfo LightInfo)
{
    UHBRANCH
    if (!SpotLight.bIsEnabled)
    {
        return 0;
    }
        
    LightInfo.LightColor = SpotLight.Color.rgb;
    LightInfo.LightDir = SpotLight.Dir;
    float3 LightToWorld = LightInfo.WorldPos - SpotLight.Position;
        
    // squared distance attenuation
    float LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + LightInfo.AttenNoise);
    LightAtten *= LightAtten;
        
    // squared spot angle attenuation
    float Rho = dot(SpotLight.Dir, normalize(LightToWorld));
    float SpotFactor = (Rho - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
    SpotFactor = saturate(SpotFactor);
        
    LightAtten *= SpotFactor * SpotFactor;
    LightInfo.ShadowMask = LightAtten * LightInfo.ShadowMask;
		
    return LightBRDF(LightInfo);
}

#endif