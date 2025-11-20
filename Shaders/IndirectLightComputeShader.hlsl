#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#define SH9_BIND t3
#include "UHSphericalHamonricCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u1);
SamplerState PointClampped : register(s4);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void IndirectLightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
    {
        return;
    }
    
    float2 UV = (DTid.xy + 0.5f) * GResolution.zw;
    float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV, 0).r;
    float4 CurrSceneData = SceneResult[DTid.xy];
    
    // don't apply lighting to empty pixels
	UHBRANCH
    if (Depth == 0.0f)
    {
        return;
    }
    
    // fetch diffuse and unpack normal
    float4 Diffuse = SceneBuffers[0].SampleLevel(PointClampped, UV, 0);
    float3 Normal = DecodeNormal(SceneBuffers[1].SampleLevel(PointClampped, UV, 0).xyz);
    
    // sample indirect lighting
    float3 Result = ShadeSH9(Diffuse.rgb, float4(Normal, 1.0f), Diffuse.a);
    
    SceneResult[DTid.xy] = float4(CurrSceneData.rgb + Result, CurrSceneData.a);
}