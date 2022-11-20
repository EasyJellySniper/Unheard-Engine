#include "UHInputs.hlsli"

static const float2 GTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

// simple VS shader for full screen quad post processing
// simply draw 6 vertex without VB/IB bound in CPU side
PostProcessVertexOutput PostProcessVS(uint vid : SV_VertexID)
{
    PostProcessVertexOutput Vout;

    Vout.UV = GTexCoords[vid];

    // Quad covering screen in NDC space.
    // make Y at left upper corner to fit Vulkan spec
    Vout.Position = float4(2.0f * Vout.UV.x - 1.0f, 2.0f * Vout.UV.y - 1.0f, 1.0f, 1.0f);

    return Vout;
}