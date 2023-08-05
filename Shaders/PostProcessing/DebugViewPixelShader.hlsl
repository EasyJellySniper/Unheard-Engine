#include "../UHInputs.hlsli"

cbuffer DebugViewConstant : register(b0)
{
	uint ViewMipLevel;
};

Texture2D TextureToDebug : register(t1);
SamplerState PointSampler : register(s2);

// a simply pass through debug view shader, just use point sampler
float4 DebugViewPS(PostProcessVertexOutput Vin) : SV_Target
{
	return TextureToDebug.SampleLevel(PointSampler, Vin.UV, ViewMipLevel);
}