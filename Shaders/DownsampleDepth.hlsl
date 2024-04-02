#include "UHInputs.hlsli"

// shader for downsampling depth buffer properly
RWTexture2D<float> Result : register(u1);
RWTexture2D<float> TranslucentResult : register(u2);
Texture2D InDepth : register(t3);
Texture2D InTranslucentDepth : register(t4);

static const uint DownsampleFactor = 2;

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void HalfDownsampleDepthCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x * DownsampleFactor >= GResolution.x || DTid.y * DownsampleFactor >= GResolution.y)
    {
        return;
    }

    // find the max depth in a 2x2 box
    float OutDepth = 0.0f;

    for (int I = 0; I <= 1; I++)
    {
        for (int J = 0; J <= 1; J++)
        {
            int2 DepthPos = DTid.xy * DownsampleFactor + int2(I, J);
            DepthPos = min(DepthPos, GResolution.xy - 1);
            OutDepth = max(OutDepth, InDepth[DepthPos].r);
        }
    }

    Result[DTid.xy] = OutDepth;

    // translucent depth
    float OutTranslucentDepth = 0.0f;

    for (I = 0; I <= 1; I++)
    {
        for (int J = 0; J <= 1; J++)
        {
            int2 DepthPos = DTid.xy * DownsampleFactor + int2(I, J);
            DepthPos = min(DepthPos, GResolution.xy - 1);
            OutTranslucentDepth = max(OutTranslucentDepth, InTranslucentDepth[DepthPos].r);
        }
    }

    TranslucentResult[DTid.xy] = OutTranslucentDepth;
}