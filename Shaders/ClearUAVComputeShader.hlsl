// simple compute shader to clear an UAV render texture
#include "UHInputs.hlsli"

RWTexture2D<float4> RenderTexture : register(u0);

struct ClearUAVConstants
{
    uint2 Resolution;
};
[[vk::push_constant]] ClearUAVConstants Constants;

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void ClearUavCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.Resolution.x || DTid.y >= Constants.Resolution.y)
    {
        return;
    }
    
    // clear as 0 for now, can carry different clear value in the future
    RenderTexture[DTid.xy] = 0.0f;
}