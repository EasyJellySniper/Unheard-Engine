// Gaussian Filter Shader, can be both blurring or sharping and base on a two pass method
#include "../UHInputs.hlsli"

// following the GThreadGroup1D setting on c++ side
#define FILTER_THREADS 64

// Max filter radius define
#ifndef MAX_FILTER_RADIUS
#define MAX_FILTER_RADIUS 5
#endif

struct GaussianFilterConstants
{
    uint2 BlurResolution;
    int BlurRadius;
    
    // Support up to 11 (Radius 5) weights
    float W0, W1, W2, W3, W4, W5, W6, W7, W8, W9, W10;
};
[[vk::push_constant]] GaussianFilterConstants Constants;

RWTexture2D<float4> OutResult : register(u0);
Texture2D InputTexture : register(t1);

groupshared float4 GColorCache[FILTER_THREADS + 2 * MAX_FILTER_RADIUS];

[numthreads(FILTER_THREADS, 1, 1)]
void HorizontalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, Constants.BlurResolution - 1);
    float BlurWeights[] = { Constants.W0, Constants.W1, Constants.W2, Constants.W3, Constants.W4, Constants.W5, Constants.W6, Constants.W7, Constants.W8, Constants.W9, Constants.W10 };
    
    // clamping the left edge for color cache
    if (GTid.x < Constants.BlurRadius)
    {
        int x = max(PixelCoord.x - Constants.BlurRadius, 0);
        GColorCache[GTid.x] = InputTexture[int2(x, PixelCoord.y)];
    }

    // clamping the right edge for color cache
    if (GTid.x >= FILTER_THREADS - Constants.BlurRadius)
    {
        int x = min(PixelCoord.x + Constants.BlurRadius, Constants.BlurResolution.x - 1);
        GColorCache[GTid.x + 2 * Constants.BlurRadius] = InputTexture[int2(x, PixelCoord.y)];
    }
    
    // start the color cache
    GColorCache[GTid.x + Constants.BlurRadius] = InputTexture[PixelCoord.xy];
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -Constants.BlurRadius; I <= Constants.BlurRadius; I++)
    {
        int KernalIndex = I + Constants.BlurRadius;
        int ColorIndex = GTid.x + KernalIndex;
        OutColor += BlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}

[numthreads(1, FILTER_THREADS, 1)]
void VerticalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, Constants.BlurResolution - 1);
    float BlurWeights[] = { Constants.W0, Constants.W1, Constants.W2, Constants.W3, Constants.W4, Constants.W5, Constants.W6, Constants.W7, Constants.W8, Constants.W9, Constants.W10 };
    
    // clamping the left edge for color cache
    if (GTid.y < Constants.BlurRadius)
    {
        int y = max(PixelCoord.y - Constants.BlurRadius, 0);
        GColorCache[GTid.y] = InputTexture[int2(PixelCoord.x, y)];
    }

    // clamping the right edge for color cache
    if (GTid.y >= FILTER_THREADS - Constants.BlurRadius)
    {
        int y = min(PixelCoord.y + Constants.BlurRadius, Constants.BlurResolution.y - 1);
        GColorCache[GTid.y + 2 * Constants.BlurRadius] = InputTexture[int2(PixelCoord.x, y)];
    }
    
    // start the color cache
    GColorCache[GTid.y + Constants.BlurRadius] = InputTexture[PixelCoord.xy];
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -Constants.BlurRadius; I <= Constants.BlurRadius; I++)
    {
        int KernalIndex = I + Constants.BlurRadius;
        int ColorIndex = GTid.y + KernalIndex;
        OutColor += BlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}