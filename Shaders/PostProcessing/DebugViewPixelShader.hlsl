#include "../UHInputs.hlsli"

Texture2D TextureToDebug : register(t0);
SamplerState PointSampler : register(s1);

// a simply pass through debug view shader, just use point sampler
float4 DebugViewPS(PostProcessVertexOutput Vin) : SV_Target
{
	return TextureToDebug.SampleLevel(PointSampler, Vin.UV, 0);
}