// Dual filtering based Kawase Blur from "Bandwidth-Efficient Rendering" SIGGRAPH 2015
#include "../UHInputs.hlsli"

struct KawaseBlurConstants
{
    uint2 OutputResolution;
};
[[vk::push_constant]] KawaseBlurConstants Constants;

RWTexture2D<float4> Result : register(u0);
Texture2D Input : register(t1);
SamplerState LinearClampped : register(s2);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void KawaseDownsampleCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.OutputResolution.x || DTid.y >= Constants.OutputResolution.y)
    {
        return;
    }

    uint2 PixelCoord = DTid.xy;
    float2 ScreenUV = float2(PixelCoord + 0.5f) / float2(Constants.OutputResolution);
    float2 HalfPixel = 1.0f / float2(Constants.OutputResolution);
    
    float4 Sum = Input.SampleLevel(LinearClampped, ScreenUV, 0) * 4.0f;
    Sum += Input.SampleLevel(LinearClampped, ScreenUV - HalfPixel, 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + HalfPixel, 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(HalfPixel.x, -HalfPixel.y), 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV - float2(HalfPixel.x, -HalfPixel.y), 0);

    // div 8
    Result[PixelCoord] = Sum * 0.125f;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void KawaseUpsampleCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.OutputResolution.x || DTid.y >= Constants.OutputResolution.y)
    {
        return;
    }
    
    uint2 PixelCoord = DTid.xy;
    float2 ScreenUV = float2(DTid.xy + 0.5f) / float2(Constants.OutputResolution);
    float2 HalfPixel = 1.0f / float2(Constants.OutputResolution);
    
    float4 Sum = Input.SampleLevel(LinearClampped, ScreenUV + float2(-HalfPixel.x * 2.0f, 0.0f), 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(-HalfPixel.x, HalfPixel.y), 0) * 2.0f;
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(0.0f, HalfPixel.y * 2.0f), 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(HalfPixel.x, HalfPixel.y), 0) * 2.0f;
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(HalfPixel.x * 2.0f, 0.0f), 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(HalfPixel.x, -HalfPixel.y), 0) * 2.0f;
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(0.0f, -HalfPixel.y * 2.0f), 0);
    Sum += Input.SampleLevel(LinearClampped, ScreenUV + float2(-HalfPixel.x, -HalfPixel.y), 0) * 2.0f;
    
    // div 12
    Result[PixelCoord] = Sum * 0.083333f;
}