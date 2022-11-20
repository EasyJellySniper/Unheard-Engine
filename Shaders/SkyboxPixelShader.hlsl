#include "UHInputs.hlsli"

struct SkyVertexOutput
{
	float4 Position : SV_POSITION;
	float3 LocalPos : TEXCOORD0;
};

TextureCube SkyCube : register(t2);
SamplerState SkySampler : register(s3);

float4 SkyboxPS(SkyVertexOutput Vin) : SV_Target
{
	// sample sky and multiply sky color
	float3 SkyColor = UHAmbientSky * SkyCube.SampleLevel(SkySampler, Vin.LocalPos, 0).rgb;
	return float4(SkyColor, 1.0f);
}