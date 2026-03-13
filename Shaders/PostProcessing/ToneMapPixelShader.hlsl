#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

cbuffer ToneMapData : register(b1)
{
    float GammaCorrection;
    float GHDRPaperWhiteNits;
    float GHDRContrast;
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

// ITU BT.709 + BT.2020 specification
static const float3x3 GRec709ToRec2020 =
{
    { 0.6274040, 0.3292820, 0.0433136 },
    { 0.0690970, 0.9195400, 0.0113612 },
    { 0.0163916, 0.0880132, 0.8955950 }
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
    // scale to display nits (div by 10000.0f)
    const float InvMaxLumin = 0.0001f;
    float3 Y = HDRColor * InvMaxLumin * GHDRPaperWhiteNits;
    Y = saturate(Y);
    
    float M1 = 0.1593017578125f;
    float M2 = 78.84375f;
    float C1 = 0.8359375f;
    float C2 = 18.8515625f;
    float C3 = 18.6875f;
    const float3 YPowM1 = pow(Y, M1);

    return pow((C1 + C2 * YPowM1) / (1 + C3 * YPowM1), M2);
}

float3 ACEToneMap(float3 InColor)
{
    float3 Result = mul(GAcesInputMat, InColor);
    Result = RRTAndODTFit(Result);
    Result = mul(GAcesOutputMat, Result);
    
    return Result;
}

float4 ToneMapPS(PostProcessVertexOutput Vin) : SV_Target
{
    float3 Result = SceneTexture.SampleLevel(SceneSampler, Vin.UV, 0).rgb;
    // debug values
    // 0.18   (middle grey)
    // 1.0    (paper white)
    // 10.0   (bright highlight)
    //Result = 10.0f;
    
    UHBRANCH
    if (GSystemRenderFeature & UH_HDR)
    {
        // HDR pipeline
        // 1. Contrast
        // 2. ACE
        // 3. Rec709 -> Rec2020
        // 4. Display nits + ST2084
        Result = pow(Result, GHDRContrast);
        Result = ACEToneMap(Result);
        Result = mul(GRec709ToRec2020, Result);
        Result = CalculateST2084(Result);
    }
    else
    {        
        Result = ACEToneMap(Result);
        // final gamma correction for SDR
        Result = pow(Result, 1.0f / GammaCorrection);
    }

    return float4(Result, 1.0f);
}