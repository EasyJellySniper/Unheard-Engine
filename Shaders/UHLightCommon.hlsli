struct UHLightInfo
{
    float3 LightColor;
    float3 LightDir;
    float3 Diffuse;
    float4 Specular;
    float3 Normal;
    float3 WorldPos;
    float ShadowMask;
};

float3 SchlickFresnel(float3 SpecColor, float LdotH)
{
    float CosIncidentAngle = LdotH;

    // pow5 F0 is used
    float F0 = 1.0f - CosIncidentAngle;
    float3 ReflectPercent = SpecColor + (1.0f - SpecColor) * (F0 * F0 * F0 * F0 * F0);

    return ReflectPercent;
}

// blinn phong
float BlinnPhong(float M, float NdotH)
{
    M = max(M, 0.01f);
    M *= 256.0f;
    float N = (M + 64.0f) / 64.0f;

    return pow(NdotH, M) * N;
}

float3 LightBRDF(UHLightInfo LightInfo)
{
    float3 LightColor = LightInfo.LightColor * LightInfo.ShadowMask;
    float3 LightDir = -LightInfo.LightDir;
    float3 ViewDir = -normalize(LightInfo.WorldPos - UHCameraPos);

    // Diffuse = N dot L
    float NdotL = saturate(dot(LightInfo.Normal, LightDir));
    float3 LightDiffuse = LightColor * NdotL;

    // Specular = SchlickFresnel * BlinnPhong * N dot L * 0.25
    // safe normalize half dir
    float3 HalfDir = (ViewDir + LightDir) / (length(ViewDir + LightDir) + UH_FLOAT_EPSILON);
    float LdotH = saturate(dot(LightDir, HalfDir));
    float NdotH = saturate(dot(LightInfo.Normal, HalfDir));

    // inverse roughness to smoothness
    float Smoothness = 1.0f - saturate(LightInfo.Specular.a);
    float3 LightSpecular = LightColor * SchlickFresnel(LightInfo.Specular.rgb, LdotH) * BlinnPhong(Smoothness, NdotH) * NdotL;

    return LightInfo.Diffuse * LightDiffuse + LightSpecular;
}

uint GetPointLightOffset(uint InIndex)
{
    return InIndex * (UHMaxPointLightPerTile * 4 + 4);
}

uint GetSpotLightOffset(uint InIndex)
{
    return InIndex * (UHMaxSpotLightPerTile * 4 + 4);
}

bool HasLighting()
{
    return UHNumDirLights > 0 || UHNumPointLights > 0 || UHNumSpotLights > 0;
}