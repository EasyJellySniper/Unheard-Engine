#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

struct DebugBoundVertexOutput
{
    float4 Position : SV_POSITION;
};

// this shader simply takes a box (or sphere) boundary and render it
cbuffer DebugBoundConstant : register(b1)
{
    float3 Position;
    uint BoundType;
    
    float3 Extent;
    float Radius;
    
    float3 Color;
};

static const float3 GBoundingBox[24] =
{
    // x lines
    float3(-1, 1, -1), float3(1, 1, -1),
    float3(-1, -1, -1), float3(1, -1, -1),
    float3(-1, 1, 1), float3(1, 1, 1),
    float3(-1, -1, 1), float3(1, -1, 1),
    
    // y lines
    float3(-1, 1, -1), float3(-1, -1, -1),
    float3(1, 1, -1), float3(1, -1, -1),
    float3(-1, 1, 1), float3(-1, -1, 1),
    float3(1, 1, 1), float3(1, -1, 1),
    
    // z lines
    float3(-1, 1, -1), float3(-1, 1, 1),
    float3(-1, -1, -1), float3(-1, -1, 1),
    float3(1, 1, -1), float3(1, 1, 1),
    float3(1, -1, -1), float3(1, -1, 1)
};

float3 GetBounding(uint Vid, uint InBoundType)
{
    UHBRANCH
    if (InBoundType == 0)
    {
        return GBoundingBox[Vid];
    }
        
    const uint NumPointsPerHalfCircle = 60;
    float T = 0.0f;
    
    // for sphere, build a xz/yz/xy circle, 120 points for each circle (360 in total)
    UHBRANCH
    if (Vid < NumPointsPerHalfCircle)
    {
        // xz circle part 1
        T = lerp(-UH_PI, UH_PI, Vid / (float)NumPointsPerHalfCircle);
        return float3(cos(T), 0.0f, sin(T));
    }
    else if (Vid < NumPointsPerHalfCircle * 2)
    {
        // xz circle part 2
        T = lerp(-UH_PI, UH_PI, (Vid % NumPointsPerHalfCircle + 1) / (float)NumPointsPerHalfCircle);
        return float3(cos(T), 0.0f, sin(T));
    }
    else if (Vid < NumPointsPerHalfCircle * 3)
    {
        // yz circle part 1
        T = lerp(-UH_PI, UH_PI, Vid / (float) NumPointsPerHalfCircle);
        return float3(0.0f, sin(T), cos(T));
    }
    else if (Vid < NumPointsPerHalfCircle * 4)
    {
        // yz circle part 2
        T = lerp(-UH_PI, UH_PI, (Vid % NumPointsPerHalfCircle + 1) / (float) NumPointsPerHalfCircle);
        return float3(0.0f, sin(T), cos(T));
    }
    else if (Vid < NumPointsPerHalfCircle * 5)
    {
        // xy circle part 1
        T = lerp(-UH_PI, UH_PI, Vid / (float) NumPointsPerHalfCircle);
        return float3(cos(T), sin(T), 0.0f);
    }
    
    // xy circle part 2
    T = lerp(-UH_PI, UH_PI, (Vid % NumPointsPerHalfCircle + 1) / (float) NumPointsPerHalfCircle);
    return float3(cos(T), sin(T), 0.0f);
}

DebugBoundVertexOutput DebugBoundVS(uint Vid : SV_VertexID)
{
    DebugBoundVertexOutput Vout = (DebugBoundVertexOutput)0;
    
    float3 Ext = (BoundType == 0) ? Extent : Radius * 0.5f;
    float3 WorldPos = GetBounding(Vid, BoundType) * Ext + Position;
    Vout.Position = mul(float4(WorldPos, 1.0f), UHViewProj_NonJittered);
    
    return Vout;
}

float4 DebugBoundPS(DebugBoundVertexOutput Vin) : SV_Target
{
    return float4(Color, 1.0f);
}