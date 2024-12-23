// shader to soften ray tracing shadows
// based on PCSS algorithm
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"
#include "../UHLightCommon.hlsli"
#include "UHRTCommon.hlsli"

RWTexture2D<float> OutRTShadow : register(u1);
Texture2D InputRTShadow : register(t2);
Texture2D DepthTexture : register(t3);
Texture2D MipRateTex : register(t4);
SamplerState PointClampped : register(s5);
SamplerState LinearClampped : register(s6);

float Shadow3x3(float2 UV, float2 BaseShadowData, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float Atten = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -1; I <= 1; I++)
    {
		UHUNROLL
        for (int J = -1; J <= 1; J++)
        {
            float2 ShadowUV = UV + float2(I, J) * Penumbra * GShadowResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, ShadowUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Atten += BaseShadowData.g;
            }
            else
            {
                Atten += InputRTShadow.SampleLevel(LinearClampped, ShadowUV, 0).g;
            }
        }
    }
    Atten *= 0.11111111f;
    
    return Atten;
}

float Shadow5x5(float2 UV, float2 BaseShadowData, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float Atten = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -2; I <= 2; I++)
    {
		UHUNROLL
        for (int J = -2; J <= 2; J++)
        {
            float2 ShadowUV = UV + float2(I, J) * Penumbra * GShadowResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, ShadowUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Atten += BaseShadowData.g;
            }
            else
            {
                Atten += InputRTShadow.SampleLevel(LinearClampped, ShadowUV, 0).g;
            }
        }
    }
    Atten *= 0.04f;
    
    return Atten;
}

float Shadow7x7(float2 UV, float2 BaseShadowData, float BaseDepth, float Penumbra, float MipWeight)
{
	// actually sampling
    float Atten = 0.0f;
    float DepthDiffThres = lerp(0.0005f, 0.0001f, MipWeight);
	
	UHUNROLL
    for (int I = -3; I <= 3; I++)
    {
		UHUNROLL
        for (int J = -3; J <= 3; J++)
        {
            float2 ShadowUV = UV + float2(I, J) * Penumbra * GShadowResolution.zw;
			
            float CmpDepth = DepthTexture.SampleLevel(PointClampped, ShadowUV, 0).r;
            if (abs(CmpDepth - BaseDepth) > DepthDiffThres)
            {
                Atten += BaseShadowData.g;
            }
            else
            {
                Atten += InputRTShadow.SampleLevel(LinearClampped, ShadowUV, 0).g;
            }
        }
    }
    Atten *= 0.02040816f;
    
    return Atten;
}

void SoftShadow(inout RWTexture2D<float> RTShadow, uint2 PixelCoord, float2 UV, float MipRate)
{
	// pre-sample the texture and cache it
    float BaseDepth = DepthTexture.SampleLevel(PointClampped, UV, 0).r;
	UHBRANCH
    if (BaseDepth == 0.0f)
    {
        return;
    }

	// after getting distance to blocker, scale it down (or not) as penumbra number
    float2 BaseShadowData = InputRTShadow.SampleLevel(LinearClampped, UV, 0).rg;
    float DistToBlocker = BaseShadowData.r;
    float Penumbra = lerp(GPCSSMinPenumbra, GPCSSMaxPenumbra, saturate(DistToBlocker * GPCSSBlockerDistScale));

	// lower the penumbra based on mip level, don't apply high penumbra at distant pixels
	float MipWeight = saturate(MipRate * RT_MIPRATESCALE);
    Penumbra = lerp(Penumbra, 0.0f, pow(MipWeight, 0.25f));

	// actually sampling, separate kernal functions as unroll for-loop performs better than a real for-loop
	float Atten = 0.0f;
    UHBRANCH
    if (GPCSSKernal == 1)
    {
        Atten = Shadow3x3(UV, BaseShadowData, BaseDepth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 2)
    {
        Atten = Shadow5x5(UV, BaseShadowData, BaseDepth, Penumbra, MipWeight);
    }
    else if (GPCSSKernal == 3)
    {
        Atten = Shadow7x7(UV, BaseShadowData, BaseDepth, Penumbra, MipWeight);
    }

    RTShadow[PixelCoord] = Atten;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void SoftRTShadowCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= GShadowResolution.x || DTid.y >= GShadowResolution.y)
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

	float2 UV = float2(PixelCoord + 0.5f) * GShadowResolution.zw;
	float MipRate = MipRateTex.SampleLevel(LinearClampped, UV, 0).r;
    SoftShadow(OutRTShadow, PixelCoord, UV, MipRate);
}