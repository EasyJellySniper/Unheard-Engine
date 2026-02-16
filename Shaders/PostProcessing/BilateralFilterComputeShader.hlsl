// Bilateral Filtering shader
// the idea was originally from Tomasi & Manduchi 1998 ¡X Bilateral Filtering.
// the implementation here is two-pass based.
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

// following the GThreadGroup1D setting on c++ side
#define FILTER_THREADS 64

// Max filter radius define
#ifndef MAX_FILTER_RADIUS
#define MAX_FILTER_RADIUS 5
#endif

struct BilateralFilterConstants
{
    uint2 FilterResolution;
    int KernalRadius;
    int SceneBufferUpsample;
    float SigmaS;
    float SigmaD;
    float SigmaN;
    float SigmaC;
};
[[vk::push_constant]] BilateralFilterConstants Constants;

RWTexture2D<float4> OutResult : register(u1);
Texture2D InputTexture : register(t2);
Texture2D SceneDepth : register(t3);
Texture2D SceneNormal : register(t4);

groupshared float4 GColorCache[FILTER_THREADS + 2 * MAX_FILTER_RADIUS];
groupshared float GDepthCache[FILTER_THREADS + 2 * MAX_FILTER_RADIUS];
groupshared float3 GNormalCache[FILTER_THREADS + 2 * MAX_FILTER_RADIUS];

float BilateralWeight(float3 Color0, float3 Color1, float3 Normal0, float3 Normal1, float Depth0, float Depth1, float Offset)
{
    // spatial term
    float Spatial = exp(-(Offset * Offset) / (2.0f * Constants.SigmaS * Constants.SigmaS));

    // depth term, calculate based on linear depth
    Depth0 = LinearDepth(Depth0);
    Depth1 = LinearDepth(Depth1);
    float DepthDiff = abs(Depth0 - Depth1);
    float Depth = exp(-(DepthDiff * DepthDiff) / (2.0f * Constants.SigmaD * Constants.SigmaD));
    
    // normal term
    float Normal = pow(saturate(dot(Normal0, Normal1)), Constants.SigmaN);
    
    // color term
    float ColorW = 1.0f;
    UHBRANCH
    if (Constants.SigmaC > 0.0f)
    {
        float3 ColorDiff = (Color0 - Color1) / max(max(Color0, Color1), 0.1f);
        ColorW = exp(-dot(ColorDiff, ColorDiff) / (2.0f * Constants.SigmaC * Constants.SigmaC));
    }

    return Spatial * Depth * Normal * ColorW;
}

[numthreads(FILTER_THREADS, 1, 1)]
void HorizontalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, Constants.FilterResolution - 1);
    
    // clamping the left edge for color cache
    if (GTid.x < Constants.KernalRadius)
    {
        int x = max(PixelCoord.x - Constants.KernalRadius, 0);
        int2 InputCoord = int2(x, PixelCoord.y);
        
        GColorCache[GTid.x] = InputTexture[InputCoord];
        GDepthCache[GTid.x] = SceneDepth[InputCoord * Constants.SceneBufferUpsample].r;
        GNormalCache[GTid.x] = DecodeNormal(SceneNormal[InputCoord * Constants.SceneBufferUpsample].xyz);
    }

    // clamping the right edge for color cache
    if (GTid.x >= FILTER_THREADS - Constants.KernalRadius)
    {
        int x = min(PixelCoord.x + Constants.KernalRadius, Constants.FilterResolution.x - 1);
        int2 InputCoord = int2(x, PixelCoord.y);
        
        GColorCache[GTid.x + 2 * Constants.KernalRadius] = InputTexture[InputCoord];
        GDepthCache[GTid.x + 2 * Constants.KernalRadius] = SceneDepth[InputCoord * Constants.SceneBufferUpsample].r;
        GNormalCache[GTid.x + 2 * Constants.KernalRadius] = DecodeNormal(SceneNormal[InputCoord * Constants.SceneBufferUpsample].xyz);
    }
    
    // store the color/normal/depth cache
    float4 CenterColor = InputTexture[PixelCoord];
    float CenterDepth = SceneDepth[PixelCoord * Constants.SceneBufferUpsample].r;
    float3 CenterNormal = DecodeNormal(SceneNormal[PixelCoord * Constants.SceneBufferUpsample].xyz);
    
    GColorCache[GTid.x + Constants.KernalRadius] = CenterColor;
    GDepthCache[GTid.x + Constants.KernalRadius] = CenterDepth;
    GNormalCache[GTid.x + Constants.KernalRadius] = CenterNormal;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    float Weight = 0.0f;
    
    for (int I = -Constants.KernalRadius; I <= Constants.KernalRadius; I++)
    {
        int KernalIndex = I + Constants.KernalRadius;
        int CacheIndex = GTid.x + KernalIndex;
        
        float W = BilateralWeight(CenterColor.rgb, GColorCache[CacheIndex].rgb
            , CenterNormal, GNormalCache[CacheIndex], CenterDepth, GDepthCache[CacheIndex], I);
        OutColor += W * GColorCache[CacheIndex];
        Weight += W;
    }
	
    OutResult[PixelCoord] = OutColor / max(Weight, 1.0f);
}

[numthreads(1, FILTER_THREADS, 1)]
void VerticalFilterCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = min(DTid.xy, Constants.FilterResolution - 1);
    
    // clamping the left edge for color cache
    if (GTid.y < Constants.KernalRadius)
    {
        int y = max(PixelCoord.y - Constants.KernalRadius, 0);
        int2 InputCoord = int2(PixelCoord.x, y);
        
        GColorCache[GTid.y] = InputTexture[InputCoord];
        GDepthCache[GTid.y] = SceneDepth[InputCoord * Constants.SceneBufferUpsample].r;
        GNormalCache[GTid.y] = DecodeNormal(SceneNormal[InputCoord * Constants.SceneBufferUpsample].xyz);
    }

    // clamping the right edge for color cache
    if (GTid.y >= FILTER_THREADS - Constants.KernalRadius)
    {
        int y = min(PixelCoord.y + Constants.KernalRadius, Constants.FilterResolution.y - 1);
        int2 InputCoord = int2(PixelCoord.x, y);
        
        GColorCache[GTid.y + 2 * Constants.KernalRadius] = InputTexture[InputCoord];
        GDepthCache[GTid.y + 2 * Constants.KernalRadius] = SceneDepth[InputCoord * Constants.SceneBufferUpsample].r;
        GNormalCache[GTid.y + 2 * Constants.KernalRadius] = DecodeNormal(SceneNormal[InputCoord * Constants.SceneBufferUpsample].xyz);
    }
    
    // store the color/normal/depth cache
    float4 CenterColor = InputTexture[PixelCoord];
    float CenterDepth = SceneDepth[PixelCoord * Constants.SceneBufferUpsample].r;
    float3 CenterNormal = DecodeNormal(SceneNormal[PixelCoord * Constants.SceneBufferUpsample].xyz);
    
    GColorCache[GTid.y + Constants.KernalRadius] = CenterColor;
    GDepthCache[GTid.y + Constants.KernalRadius] = CenterDepth;
    GNormalCache[GTid.y + Constants.KernalRadius] = CenterNormal;
    GroupMemoryBarrierWithGroupSync();
    
    float4 OutColor = float4(0, 0, 0, 0);
    float Weight = 0.0f;
    
    for (int I = -Constants.KernalRadius; I <= Constants.KernalRadius; I++)
    {
        int KernalIndex = I + Constants.KernalRadius;
        int CacheIndex = GTid.y + KernalIndex;
        
        float W = BilateralWeight(CenterColor.rgb, GColorCache[CacheIndex].rgb
            , CenterNormal, GNormalCache[CacheIndex], CenterDepth, GDepthCache[CacheIndex], I);
        OutColor += W * GColorCache[CacheIndex];
        Weight += W;
    }
	
    OutResult[PixelCoord] = OutColor / max(Weight, 1.0f);
}