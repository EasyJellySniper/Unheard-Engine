#include "../UHInputs.hlsli"

// Gaussian Blur shader, based on a two pass method
RWTexture2D<float4> OutResult : register(u1);
Texture2D InputTexture : register(t2);

// following the GThreadGroup1D setting on c++ side
#define BLUR_THREADS 64
#define BLUR_RADIUS 4

// Use a fixed kernal for now, the weights are pre-calculated based on the following lines:
// Sigma2 = 2.0f * Sigma * Sigma;
// BlurRadius = ceil(2.0f * sigma);
// Loop I = -BlurRadius to BlurRadius
	// X = I
	// Weights[I] = expf(-X*X/Sigma2);
	// WeightSum += Weights[I]
// Loop I = 0 to Weights.size()
	// Weights[I] / WeightSum

static float GBlurWeights[9] = { 0.0276305489f, 0.0662822425f, 0.123831533f, 0.180173814f, 0.204163685f, 0.180173814f, 0.123831533f, 0.0662822425f, 0.0276305489f };
groupshared float3 GColorCache[BLUR_THREADS + 2 * BLUR_RADIUS];

struct UHGaussianBlurData
{
    uint2 BlurResolution;
};
[[vk::push_constant]] UHGaussianBlurData GaussianBlurData;

[numthreads(BLUR_THREADS, 1, 1)]
void HorizontalBlurCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GaussianBlurData.BlurResolution - 1);
    
    // clamping the left edge for color cache
    if (GTid.x < BLUR_RADIUS)
    {
        int x = max(PixelCoord.x - BLUR_RADIUS, 0);
        GColorCache[GTid.x] = InputTexture[int2(x, PixelCoord.y)].rgb;
    }

    // clamping the right edge for color cache
    if (GTid.x >= BLUR_THREADS - BLUR_RADIUS)
    {
        int x = min(PixelCoord.x + BLUR_RADIUS, UHResolution.x - 1);
        GColorCache[GTid.x + 2 * BLUR_RADIUS] = InputTexture[int2(x, PixelCoord.y)].rgb;
    }
    
    // start the color cache
    GColorCache[GTid.x + BLUR_RADIUS] = InputTexture[PixelCoord.xy].rgb;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -BLUR_RADIUS; I <= BLUR_RADIUS; I++)
    {
        int KernalIndex = I + BLUR_RADIUS;
        int ColorIndex = GTid.x + KernalIndex;
        OutColor.rgb += GBlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}

[numthreads(1, BLUR_THREADS, 1)]
void VerticalBlurCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, GaussianBlurData.BlurResolution - 1);
    
    // clamping the left edge for color cache
    if (GTid.y < BLUR_RADIUS)
    {
        int y = max(PixelCoord.y - BLUR_RADIUS, 0);
        GColorCache[GTid.y] = InputTexture[int2(PixelCoord.x, y)].rgb;
    }

    /// clamping the right edge for color cache
    if (GTid.y >= BLUR_THREADS - BLUR_RADIUS)
    {
        int y = min(PixelCoord.y + BLUR_RADIUS, UHResolution.y - 1);
        GColorCache[GTid.y + 2 * BLUR_RADIUS] = InputTexture[int2(PixelCoord.x, y)].rgb;
    }
    
    // start the color cache
    GColorCache[GTid.y + BLUR_RADIUS] = InputTexture[PixelCoord.xy].rgb;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    for (int I = -BLUR_RADIUS; I <= BLUR_RADIUS; I++)
    {
        int KernalIndex = I + BLUR_RADIUS;
        int ColorIndex = GTid.y + KernalIndex;
        OutColor.rgb += GBlurWeights[KernalIndex] * GColorCache[ColorIndex];
    }
	
    OutResult[DTid.xy] = OutColor;
}