#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

cbuffer ToneMapData : register(b1)
{
    float HDRPaperWhiteNits;
    float HDRContrast;
};

Texture2D SceneTexture : register(t2);
SamplerState SceneSampler : register(s3);

// The code in this file was originally written by Stephen Hill (@self_shadow), who deserves all
// credit for coming up with this fit and implementing it.

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 GAcesInputMat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 GAcesOutputMat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float3 RRTAndODTFit(float3 V)
{
    float3 A = V * (V + 0.0245786f) - 0.000090537f;
    float3 B = V * (0.983729f * V + 0.4329510f) + 0.238081f;
    return A / B;
}

// ST2084 curve calculation, reference: https://en.wikipedia.org/wiki/Perceptual_quantizer
float3 CalculateST2084(float3 HDRColor)
{
    // normalize the HDR color
    const float InvMaxLumin = 0.0001f;
    float3 Y = HDRColor * InvMaxLumin * HDRPaperWhiteNits;
    
    float M1 = 0.1593017578125f;
    float M2 = 78.84375f;
    float C1 = 0.8359375f;
    float C2 = 18.8515625f;
    float C3 = 18.6875f;
    const float3 YPowM1 = pow(Y, M1);

    return pow((C1 + C2 * YPowM1) / (1 + C3 * YPowM1), M2);
}

float4 ToneMapPS(PostProcessVertexOutput Vin) : SV_Target
{
    float3 Result = SceneTexture.SampleLevel(SceneSampler, Vin.UV, 0).rgb;
    
    UHBRANCH
    if (GSystemRenderFeature & UH_HDR)
    {
        // if it's HDR, apply contrast and convert to ST2084 standard
        Result = max(Result, UH_FLOAT_EPSILON);
        Result = pow(Result, HDRContrast);
        Result = CalculateST2084(Result);
    }
    else
    {
        Result = mul(GAcesInputMat, Result);
        Result = RRTAndODTFit(Result);
        Result = mul(GAcesOutputMat, Result);
        Result = saturate(Result);
    }

    return float4(Result, 1.0f);
}