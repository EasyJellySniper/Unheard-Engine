// shader to reproject RT indirect tracing result
// the pass should be added before Bilateral filtering
#include "../UHInputs.hlsli"
#include "../UHCommon.hlsli"

RWTexture2D<float4> OutResult : register(u1);
Texture2D HistoryResult : register(t2);
Texture2D SceneMotion : register(t3);
Texture2D SceneDepth : register(t4);
Texture2D SceneNormal : register(t5);
Texture2D HistoryDepth : register(t6);
Texture2D HistoryNormal : register(t7);

SamplerState PointClampped : register(s8);
SamplerState LinearClampped : register(s9);

struct RTIndirectReprojectionConstants
{
    uint2 Resolution;
    float AlphaMin;
    float AlphaMax;
};
[[vk::push_constant]] RTIndirectReprojectionConstants Constants;

static const float GDepthThresholdScale = 0.3f;
static const float GNormalThreshold = 0.75f;

struct RTReprojectionData
{
    float4 CurrResult;
    float CurrDepth;
    float3 CurrNormal;
    float4 PrevResult;
    float PrevDepth;
    float3 PrevNormal;
};

RTReprojectionData GetReprojectionData(uint2 PixelCoord, float2 ScreenUV)
{
    RTReprojectionData Data = (RTReprojectionData)0;
    
    Data.CurrResult = OutResult[PixelCoord];
    Data.CurrDepth = SceneDepth.SampleLevel(PointClampped, ScreenUV, 0).r;
    Data.CurrNormal = DecodeNormal(SceneNormal.SampleLevel(LinearClampped, ScreenUV, 0).xyz);

    // load previous result for temporal accumulation and consider motion vector
    float2 Motion = SceneMotion.SampleLevel(PointClampped, ScreenUV, 0).rg;
    Motion = clamp(Motion, -0.05, 0.05);
    float2 HistoryUV = ScreenUV - Motion;
    
    Data.PrevResult = 0;
    Data.PrevDepth = 0;
    Data.PrevNormal = 0;
    
    UHBRANCH
    if (IsUVInsideScreen(HistoryUV))
    {
        Data.PrevResult = HistoryResult.SampleLevel(LinearClampped, HistoryUV, 0);
        Data.PrevDepth = HistoryDepth.SampleLevel(LinearClampped, HistoryUV, 0).r;
        Data.PrevNormal = DecodeNormal(HistoryNormal.SampleLevel(LinearClampped, HistoryUV, 0).xyz);
    }
    else
    {
        Data.PrevResult = Data.CurrResult;
        Data.PrevDepth = Data.CurrDepth;
        Data.PrevNormal = Data.CurrNormal;
    }
    
    return Data;
}

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void RTSkyReprojectionCS(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Constants.Resolution.x || DTid.y >= Constants.Resolution.y)
    {
        return;
    }
    
    uint2 PixelCoord = DTid.xy;
    float2 ScreenUV = (float2(PixelCoord) + 0.5f) / float2(Constants.Resolution);
    
    // fetch data
    RTReprojectionData ReproData = GetReprojectionData(PixelCoord, ScreenUV);
    
    float Confidence = max(ReproData.CurrResult.g, ReproData.PrevResult.g);
    float AlphaBright = Constants.AlphaMax;
    float AlphaDark = Constants.AlphaMin;
    AlphaBright *= (1 - Confidence);
    AlphaDark *= (1 - Confidence);
    
    // prefer bright pixels for the result, this fill black holes more
    // also clamp prev visibility a bit to reduce lag behind sudden lighting
    float ClampRange = 0.2f;
    float MinClamp = max(0, ReproData.CurrResult.r - ClampRange);
    float MaxClamp = min(1, ReproData.CurrResult.r + ClampRange);
    
    ReproData.PrevResult.r = clamp(ReproData.PrevResult.r, MinClamp, MaxClamp);
    float Alpha = ReproData.CurrResult.r > ReproData.PrevResult.r ? AlphaBright : AlphaDark;
    
    // out of bound, depth and normal rejection
    float DepthDiff = abs(ReproData.CurrDepth - ReproData.PrevDepth);
    float DepthThreshold = max(0.001f, ReproData.PrevDepth * GDepthThresholdScale);
    float NormalDiff = dot(ReproData.CurrNormal, ReproData.PrevNormal);
    
    bool bDepthReject = DepthDiff > DepthThreshold;
    bool bNormalReject = NormalDiff < GNormalThreshold;
    
    UHBRANCH
    if (bDepthReject || bNormalReject || (ReproData.CurrDepth == 0.0f))
    {
        // reset both visibility and confidence
        ReproData.PrevResult.r = ReproData.CurrResult.r;
        ReproData.PrevResult.g = min(ReproData.PrevResult.g, 0.25f);
    }
    
    // final blending and output
    OutResult[PixelCoord].rg = lerp(ReproData.PrevResult.rg, ReproData.CurrResult.rg, saturate(Alpha));
}

groupshared float3 GColorCache[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void RTDiffuseReprojectionCS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x >= Constants.Resolution.x || DTid.y >= Constants.Resolution.y)
    {
        return;
    }
    
    int2 PixelCoord = DTid.xy;
    float2 ScreenUV = (float2(PixelCoord) + 0.5f) / float2(Constants.Resolution);
    
    // fetch data
    RTReprojectionData ReproData = GetReprojectionData(PixelCoord, ScreenUV);
    GColorCache[GTid.x + GTid.y * UHTHREAD_GROUP2D_X] = ReproData.CurrResult.rgb;
    GroupMemoryBarrierWithGroupSync();
    
    // energy-scaled clamp
    float3 ClampRange = max(ReproData.CurrResult.rgb * 0.5f, 0.1f);
    float3 MinClamp = ReproData.CurrResult.rgb - ClampRange;
    float3 MaxClamp = ReproData.CurrResult.rgb + ClampRange;
    ReproData.PrevResult.rgb = clamp(ReproData.PrevResult.rgb, MinClamp, MaxClamp);
    
    // 3x3 color clamping
    float3 MaxColor = -UH_FLOAT_MAX;
    float3 MinColor = UH_FLOAT_MAX;

	UHUNROLL
    for (int Idx = -1; Idx <= 1; Idx++)
    {
		UHUNROLL
        for (int Jdx = -1; Jdx <= 1; Jdx++)
        {
            int2 Pos = min(int2(GTid.xy) + int2(Idx, Jdx), int2(UHTHREAD_GROUP2D_X - 1, UHTHREAD_GROUP2D_Y - 1));
            Pos = max(Pos, 0);

            float3 Color = GColorCache[Pos.x + Pos.y * UHTHREAD_GROUP2D_X];
            MinColor = min(MinColor, Color);
            MaxColor = max(MaxColor, Color);
        }
    }
    ReproData.PrevResult.rgb = clamp(ReproData.PrevResult.rgb, MinColor, MaxColor);
    
    // out of bound, depth and normal rejection
    float DepthDiff = abs(ReproData.CurrDepth - ReproData.PrevDepth);
    float DepthThreshold = max(0.001f, ReproData.PrevDepth * GDepthThresholdScale);
    float NormalDiff = dot(ReproData.CurrNormal, ReproData.PrevNormal);
    
    bool bDepthReject = DepthDiff > DepthThreshold;
    bool bNormalReject = NormalDiff < GNormalThreshold;
    
    // lumin adaptive alpha
    float CurrLumin = RGBToLuminance(ReproData.CurrResult.rgb);
    float PrevLumin = RGBToLuminance(ReproData.PrevResult.rgb);
    float LuminDiff = abs(CurrLumin - PrevLumin);
    float Variance = LuminDiff / max(max(CurrLumin, PrevLumin), 0.001);
    float Alpha = lerp(Constants.AlphaMin, Constants.AlphaMax, saturate(Variance));
    
    UHBRANCH
    if (bDepthReject || bNormalReject || (ReproData.CurrDepth == 0.0f))
    {
        // reset result diffuse when rejected
        ReproData.PrevResult = ReproData.CurrResult;
    }
    
    // final blending and output
    OutResult[PixelCoord] = lerp(ReproData.PrevResult, ReproData.CurrResult, saturate(Alpha));
}