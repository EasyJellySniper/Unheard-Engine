// Gaussian Filter Shader, can be both blurring or sharping and base on a two pass method
#include "../UHInputs.hlsli"

// following the GThreadGroup1D setting on c++ side
#define FILTER_THREADS 64

// Max filter radius define
#ifndef MAX_FILTER_RADIUS
#define MAX_FILTER_RADIUS 5
#endif

cbuffer GaussianFilterConstants : register(b1)
{
    uint2 GBlurResolution;
    int GBlurRadius;
    
    // Support up to 11 (Radius 5) weights
    float W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10;
};

RWTexture2D<float4> OutResult : register(u2);
Texture2D InputTexture : register(t3);

groupshared float3 GColorCache[FILTER_THREADS + 2 * MAX_FILTER_RADIUS];

[numthreads(FILTER_THREADS, 1, 1)]
void HorizontalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GBlurResolution - 1);
    float BlurWeights[] = { W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10 };
    
    // clamping the left edge for color cache
    if (GTid.x < GBlurRadius)
    {
        int x = max(PixelCoord.x - GBlurRadius, 0);
        GColorCache[GTid.x] = InputTexture[int2(x, PixelCoord.y)].rgb;
    }

    // clamping the right edge for color cache
    if (GTid.x >= FILTER_THREADS - GBlurRadius)
    {
        int x = min(PixelCoord.x + GBlurRadius, UHResolution.x - 1);
        GColorCache[GTid.x + 2 * GBlurRadius] = InputTexture[int2(x, PixelCoord.y)].rgb;
    }
    
    // start the color cache
    GColorCache[GTid.x + GBlurRadius] = InputTexture[PixelCoord.xy].rgb;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -GBlurRadius; I <= GBlurRadius; I++)
    {
        int KernalIndex = I + GBlurRadius;
        int ColorIndex = GTid.x + KernalIndex;
        OutColor.rgb += BlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}

[numthreads(1, FILTER_THREADS, 1)]
void VerticalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GBlurResolution - 1);
    float BlurWeights[] = { W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10 };
    
    // clamping the left edge for color cache
    if (GTid.y < GBlurRadius)
    {
        int y = max(PixelCoord.y - GBlurRadius, 0);
        GColorCache[GTid.y] = InputTexture[int2(PixelCoord.x, y)].rgb;
    }

    /// clamping the right edge for color cache
    if (GTid.y >= FILTER_THREADS - GBlurRadius)
    {
        int y = min(PixelCoord.y + GBlurRadius, UHResolution.y - 1);
        GColorCache[GTid.y + 2 * GBlurRadius] = InputTexture[int2(PixelCoord.x, y)].rgb;
    }
    
    // start the color cache
    GColorCache[GTid.y + GBlurRadius] = InputTexture[PixelCoord.xy].rgb;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -GBlurRadius; I <= GBlurRadius; I++)
    {
        int KernalIndex = I + GBlurRadius;
        int ColorIndex = GTid.y + KernalIndex;
        OutColor.rgb += BlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}