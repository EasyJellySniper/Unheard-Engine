#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "UHLightCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"

// might support multiple cube in the future
RWTexture2D<float4> SceneResult : register(u1);

TextureCube EnvCube : register(t3);
Texture2D RTReflection : register(t4);
Texture2D RealtimeAO : register(t5);

SamplerState EnvSampler : register(s6);
SamplerState PointClampped : register(s7);
SamplerState LinearClampped : register(s8);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void ReflectionCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= GResolution.x || DTid.y >= GResolution.y)
    {
        return;
    }
    
    float2 UV = (DTid.xy + 0.5f) * GResolution.zw;
    float Depth = SceneBuffers[3].SampleLevel(PointClampped, UV, 0).r;
    float4 CurrSceneData = SceneResult[DTid.xy];
    
    // early return if no depth
    UHBRANCH
    if (Depth == 0.0f)
    {
        return;
    }
    
    float3 WorldPos = ComputeWorldPositionFromDeviceZ(float2(DTid.xy + 0.5f), Depth);
    float3 BumpNormal = DecodeNormal(SceneBuffers[1].SampleLevel(LinearClampped, UV, 0).xyz);
    float4 Specular = SceneBuffers[2].SampleLevel(LinearClampped, UV, 0);
    
    // merge RTAO and material AO
    float RTAO = RealtimeAO.SampleLevel(LinearClampped, UV, 0).r;
    float Occlusion = SceneBuffers[0].SampleLevel(LinearClampped, UV, 0).a;
    Occlusion = min(Occlusion, RTAO);
    
    // use 1.0f - smooth * smooth as mip bias, so it will blurry with low smoothness
    float SpecFade = Specular.a * Specular.a;
    float SpecMip = (1.0f - SpecFade) * GEnvCubeMipMapCount;
    float3 IndirectSpecular = 0;
    float3 EyeVector = normalize(WorldPos - GCameraPos);
    
    if ((GSystemRenderFeature & UH_ENV_CUBE))
    {
        float3 R = reflect(EyeVector, BumpNormal);
        IndirectSpecular = EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * GAmbientSky;
    }
    
    // calc n dot v for fresnel
    float NdotV = abs(dot(BumpNormal, -EyeVector));
    // CurrSceneData.a = fresnel factor
    float3 Fresnel = SchlickFresnel(Specular.rgb, lerp(0, NdotV, CurrSceneData.a));
    
    // reflection from dynamic source (such as ray tracing)
    SpecMip = (1.0f - SpecFade) * GRTReflectionMipCount;
    float4 DynamicReflection = RTReflection.SampleLevel(LinearClampped, UV, SpecMip);
    IndirectSpecular = lerp(IndirectSpecular, DynamicReflection.rgb, DynamicReflection.a);
    IndirectSpecular *= SpecFade * Fresnel * Occlusion * GFinalReflectionStrength;
    
    // add indirect specular
    SceneResult[DTid.xy] = float4(CurrSceneData.rgb + IndirectSpecular, CurrSceneData.a);
}