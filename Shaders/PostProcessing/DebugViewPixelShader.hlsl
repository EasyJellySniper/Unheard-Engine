#include "../UHInputs.hlsli"

cbuffer DebugViewConstant : register(b0)
{
	uint ViewMipLevel;
    uint ViewAlpha;
};

Texture2D TextureToDebug : register(t1);
SamplerState PointSampler : register(s2);

// a simply pass through debug view shader, just use point sampler
float4 DebugViewPS(PostProcessVertexOutput Vin) : SV_Target
{
    if (ViewAlpha == 1)
    {
        return TextureToDebug.SampleLevel(PointSampler, Vin.UV, ViewMipLevel).a;
    }
	return TextureToDebug.SampleLevel(PointSampler, Vin.UV, ViewMipLevel);
}