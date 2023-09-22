#include "../UHInputs.hlsli"

Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s1);

cbuffer ToneMapConstant : register(b2)
{
    bool bIsHDR;
}

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

float4 ToneMapPS(PostProcessVertexOutput Vin) : SV_Target
{
    float3 Result = SceneTexture.SampleLevel(SceneSampler, Vin.UV, 0).rgb;
    
    UHBRANCH
    if (bIsHDR)
    {
        // apply a log10 and gamma conversion
        Result = log10(Result + 1);
        Result = pow(Result, 0.454545f);
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