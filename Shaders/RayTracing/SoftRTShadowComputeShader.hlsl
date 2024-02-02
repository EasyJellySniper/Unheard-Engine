// shader to soften ray tracing shadows
// based on PCSS algorithm
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "UHRTCommon.hlsli"

RWTexture2D<float> OutRTShadow : register(u1);
Texture2D InputRTShadow : register(t2);
Texture2D DepthTexture : register(t3);
Texture2D TranslucentDepthTexture : register(t4);
Texture2D MipRateTex : register(t5);
SamplerState PointClampped : register(s6);
SamplerState LinearClampped : register(s7);

// hard code to 5x5 for now, for different preset, set different define from C++ side in the future
#define PCSS_INNERLOOP 2
#define PCSS_WEIGHT 0.04f
#define PCSS_MINPENUMBRA 1.5f
#define PCSS_MAXPENUMBRA 10.0f
#define PCSS_BLOCKERSCALE 0.02f

void SoftShadow(inout RWTexture2D<float> RTShadow, uint2 PixelCoord, float2 UV, float MipRate, float OpaqueDepth, bool bIsTranslucent)
{
	// pre-sample the texture and cache it
    float BaseDepth = (bIsTranslucent) ? TranslucentDepthTexture.SampleLevel(PointClampped, UV, 0).r : DepthTexture.SampleLevel(PointClampped, UV, 0).r;
	UHBRANCH
    if (BaseDepth == 0.0f)
    {
        return;
    }
	
    if (bIsTranslucent && BaseDepth == OpaqueDepth)
    {
		// the result of opaque & translucent shadow is stored in the same buffer now
		// return if it's already processed
        return;
    }

	// after getting distance to blocker, scale it down (or not) as penumbra number
    float2 BaseShadowData = InputRTShadow.SampleLevel(LinearClampped, UV, 0).rg;
    float DistToBlocker = BaseShadowData.r;
	float Penumbra = lerp(PCSS_MINPENUMBRA, PCSS_MAXPENUMBRA, saturate(DistToBlocker * PCSS_BLOCKERSCALE));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
	float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
    Penumbra = lerp(Penumbra, 1.0f, pow(MipWeight, 0.25f));

	// actually sampling
	float Atten = 0.0f;
	float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
	for (int I = -PCSS_INNERLOOP; I <= PCSS_INNERLOOP; I++)
	{
		UHUNROLL
		for (int J = -PCSS_INNERLOOP; J <= PCSS_INNERLOOP; J++)
		{
            float2 ShadowUV = UV + float2(I, J) * Penumbra * RTShadowResolution.zw;
			
            float CmpDepth = (bIsTranslucent) ? TranslucentDepthTexture.SampleLevel(PointClampped, ShadowUV, 0).r : DepthTexture.SampleLevel(PointClampped, ShadowUV, 0).r;
            if ((CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Atten += BaseShadowData.g;
            }
			else
            {
                Atten += InputRTShadow.SampleLevel(LinearClampped, ShadowUV, 0).g;
            }
        }
	}
	Atten *= PCSS_WEIGHT;

    RTShadow[PixelCoord] = Atten;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void SoftRTShadowCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= RTShadowResolution.x || DTid.y >= RTShadowResolution.y)
	{
		return;
	}
	
    uint2 PixelCoord = DTid.xy;
    OutRTShadow[PixelCoord] = 0;

	// early leave if there is no lights
	UHBRANCH
	if (!HasLighting())
	{
		return;
	}

	float2 UV = float2(PixelCoord + 0.5f) * RTShadowResolution.zw;
	float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;
    float OpaqueDepth = DepthTexture.SampleLevel(PointClampped, UV, 0).r;

    SoftShadow(OutRTShadow, PixelCoord, UV, MipRate, OpaqueDepth, false);
    SoftShadow(OutRTShadow, PixelCoord, UV, MipRate, OpaqueDepth, true);
}