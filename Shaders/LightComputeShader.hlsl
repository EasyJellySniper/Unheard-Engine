#define UHDIRLIGHT_BIND t1
#define UHGBUFFER_BIND t2
#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "RayTracing/UHRTCommon.hlsli"

RWTexture2D<float4> SceneResult : register(u3);
SamplerState LinearClampped : register(s4);
Texture2D RTShadow : register(t5);

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void LightCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
	if (DTid.x >= UHResolution.x || DTid.y >= UHResolution.y)
	{
		return;
	}

	float Depth = SceneBuffers[3][DTid.xy].r;
    float3 CurrSceneColor = SceneResult[DTid.xy].rgb;

	// don't apply lighting to empty pixels or there is no light
	UHBRANCH
	if (Depth == 0.0f || UHNumDirLights == 0)
	{
		return;
	}

	float2 UV = float2(DTid.xy + 0.5f) * UHResolution.zw;

	// reconstruct world position
	float3 WorldPos = ComputeWorldPositionFromDeviceZ(DTid.xy, Depth);

	// get diffuse color, a is occlusion
	float4 Diffuse = SceneBuffers[0][DTid.xy];

	// unpack normal from [0,1] to [-1,1]
	float3 Normal = SceneBuffers[1][DTid.xy].xyz;
	Normal = Normal * 2.0f - 1.0f;

	// get specular color, a is roughness
	float4 Specular = SceneBuffers[2][DTid.xy];

	float3 Result = 0;

	// sample shadows
	float ShadowMask = 1.0f;
#if WITH_RTSHADOWS
	ShadowMask = RTShadow.SampleLevel(LinearClampped, UV, 0).g;
#endif

	// dir lights accumulation
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
		Result += LightBRDF(UHDirLights[Ldx], Diffuse.rgb, Specular, Normal, WorldPos, ShadowMask);
	}

	// indirect light accumulation
	Result += LightIndirect(Diffuse.rgb, Normal, Diffuse.a);

    SceneResult[DTid.xy] = float4(CurrSceneColor + Result, 0);
}