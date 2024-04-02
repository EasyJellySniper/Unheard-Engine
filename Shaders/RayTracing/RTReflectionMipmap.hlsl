// shader to generate mipmaps for RT reflection
// it can't just use the regular box average downsample for some reason
// instead, RT reflection generates the mipmaps with max filter
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

RWTexture2D<float4> InputMip : register(u0);
RWTexture2D<float4> OutputMip : register(u1);

struct MipmapConstants
{
    uint MipWidth;
    uint MipHeight;
    uint bUseAlphaWeight;
};
[[vk::push_constant]] MipmapConstants Constants;

groupshared float4 GColorCache[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void MaxFilterMipmapCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 InputResolution = uint2(Constants.MipWidth, Constants.MipHeight) * 2;

    // cache input pixels
    GColorCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = InputMip[min(DTid.xy * 2, InputResolution.xy - 1)];
    GroupMemoryBarrierWithGroupSync();

    // downsample
    float4 OutputColor = 0;
    float Weight = 0;

    for (int I = 0; I < 2; I++)
    {
        for (int J = 0; J < 2; J++)
        {
            int2 Pos = min(int2(GTid.xy) + int2(I, J), int2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
            Pos = max(Pos, 0);
            
            float4 Color = GColorCache[Pos.x + Pos.y * UHTHREAD_GROUP2D_X];
            UHBRANCH
            if (Constants.bUseAlphaWeight)
            {
                OutputColor += Color * Color.a;
                Weight += Color.a;
            }
            else
            {
                OutputColor += Color;
                Weight += 1;
            }
        }
    }
    
    OutputColor /= max(Weight, 1);
    OutputMip[DTid.xy] = OutputColor;
}