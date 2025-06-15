#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

// compute shader for image upsample, might contain various methods
RWTexture2D<float4> Result : register(u0);

struct UpsampleConstants
{
    uint2 OutputResolution;
};
[[vk::push_constant]] UpsampleConstants Constants;

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void UpsampleNearest2x2CS(uint3 DTid : SV_DispatchThreadID)
{
    // 2x2 NMM, simply duplicate the same pixel within a 2x2 box
    //   0 1
    // 0 o x
    // 1 x x
    // Pixels with 'x' will duplicate the 'o'.
    int2 Pos = DTid.xy;
    
    // Early return for the source pixel
    if ((DTid.x & 1) == 0 && (DTid.y & 1) == 0)
    {
        return;
    }
    
    // Evaluate destination pixel
    if ((DTid.x & 1) == 1 && (DTid.y & 1) == 0)
    {
        Pos -= int2(1, 0);
    }
    else if ((DTid.x & 1) == 0 && (DTid.y & 1) == 1)
    {
        Pos -= int2(0, 1);
    }
    else if ((DTid.x & 1) == 1 && (DTid.y & 1) == 1)
    {
        Pos -= int2(1, 1);
    }
    
    Pos = min(Pos, Constants.OutputResolution.xy - 1);
    Result[DTid.xy] = Result[Pos];
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void UpsampleNearestHorizontalCS(uint3 DTid : SV_DispatchThreadID)
{
    // Horizontal NMM, simply duplicate the same pixel from [-1,0]
    //   0 1
    // 0 o x
    // 1 o x
    // 2 o x
    // 3 o x
    
    // Early return for the source pixel
    if ((DTid.x & 1) == 0)
    {
        return;
    }
    
    int2 Pos = DTid.xy - uint2(1, 0);
    Pos = min(Pos, Constants.OutputResolution.xy - 1);
    Result[DTid.xy] = Result[Pos];
}