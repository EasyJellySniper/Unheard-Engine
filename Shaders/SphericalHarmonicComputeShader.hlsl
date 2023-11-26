#include "UHInputs.hlsli"
#include "UHCommon.hlsli"
#include "UHSphericalHamonricCommon.hlsli"

RWStructuredBuffer<UHSphericalHarmonicData> Output : register(u0);
cbuffer UHSphericalHarmonicConsts : register(b1)
{
	uint GMipLevel;
	float GWeight; // this should be set as 4.0f * PI / SampleCount in C++ side
};

TextureCube EnvCube : register(t2);
SamplerState EnvSampler : register(s3);

groupshared UHSphericalHarmonicVector3 GSH3_R[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];
groupshared UHSphericalHarmonicVector3 GSH3_G[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];
groupshared UHSphericalHarmonicVector3 GSH3_B[UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y];

// 64 threads will be activated for this task, that is, C++ side is calling Dispatch(1,1,1)
[numthreads(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y, 1)]
void GenerateSHParameterCS(uint3 DTid : SV_DispatchThreadID, uint GIndex : SV_GroupIndex)
{
	// uniformly sample the sphere
	float2 UV = float2(DTid.xy + 0.5f) / float2(UHTHREAD_GROUP2D_X, UHTHREAD_GROUP2D_Y);
	float3 SampleDir = ConvertUVToSpherePos(UV);
	float3 SkyColor = EnvCube.SampleLevel(EnvSampler, SampleDir, GMipLevel).rgb;

	// calculate SHBasis
	UHSphericalHarmonicVector3 SH3Vector = SHBasis3(SampleDir);
	UHSphericalHarmonicVector3 SH3_R = SH3Vector;
	UHSphericalHarmonicVector3 SH3_G = SH3Vector;
	UHSphericalHarmonicVector3 SH3_B = SH3Vector;

	MultiplySH3(SH3_R, SkyColor.r * GWeight);
	MultiplySH3(SH3_G, SkyColor.g * GWeight);
	MultiplySH3(SH3_B, SkyColor.b * GWeight);

	GSH3_R[GIndex] = SH3_R;
	GSH3_G[GIndex] = SH3_G;
	GSH3_B[GIndex] = SH3_B;
	GroupMemoryBarrierWithGroupSync();

	// sum up all SH3 parameters and calculate the SH data
	if (GIndex == 0)
	{
		SH3_R = (UHSphericalHarmonicVector3)0;
		SH3_G = (UHSphericalHarmonicVector3)0;
		SH3_B = (UHSphericalHarmonicVector3)0;

		for (int Idx = 0; Idx < UHTHREAD_GROUP2D_X * UHTHREAD_GROUP2D_Y; Idx++)
		{
			AddSH3(SH3_R, GSH3_R[Idx]);
			AddSH3(SH3_G, GSH3_G[Idx]);
			AddSH3(SH3_B, GSH3_B[Idx]);
		}

		float SqrtPI = sqrt(UH_PI);
		float FC0 = 1.0f / (2.0f * SqrtPI);
		float FC1 = sqrt(3.0f) / (3.0f * SqrtPI);
		float FC2 = sqrt(15.0f) / (8.0f * SqrtPI);
		float FC3 = sqrt(5.0f) / (16.0f * SqrtPI);
		float FC4 = 0.5f * FC2;

		UHSphericalHarmonicData OutData = (UHSphericalHarmonicData)0;
		OutData.cAr.x = -FC1 * SH3_R.V0[3];
		OutData.cAr.y = -FC1 * SH3_R.V0[1];
		OutData.cAr.z = FC1 * SH3_R.V0[2];
		OutData.cAr.w = FC0 * SH3_R.V0[0] - FC3 * SH3_R.V1[2];

		OutData.cAg.x = -FC1 * SH3_G.V0[3];
		OutData.cAg.y = -FC1 * SH3_G.V0[1];
		OutData.cAg.z = FC1 * SH3_G.V0[2];
		OutData.cAg.w = FC0 * SH3_G.V0[0] - FC3 * SH3_G.V1[2];

		OutData.cAb.x = -FC1 * SH3_B.V0[3];
		OutData.cAb.y = -FC1 * SH3_B.V0[1];
		OutData.cAb.z = FC1 * SH3_B.V0[2];
		OutData.cAb.w = FC0 * SH3_B.V0[0] - FC3 * SH3_B.V1[2];

		OutData.cBr.x = FC2 * SH3_R.V1[0];
		OutData.cBr.y = -FC2 * SH3_R.V1[1];
		OutData.cBr.z = 3.0f * FC3 * SH3_R.V1[2];
		OutData.cBr.w = -FC2 * SH3_R.V1[3];

		OutData.cBg.x = FC2 * SH3_G.V1[0];
		OutData.cBg.y = -FC2 * SH3_G.V1[1];
		OutData.cBg.z = 3.0f * FC3 * SH3_G.V1[2];
		OutData.cBg.w = -FC2 * SH3_G.V1[3];

		OutData.cBb.x = FC2 * SH3_B.V1[0];
		OutData.cBb.y = -FC2 * SH3_B.V1[1];
		OutData.cBb.z = 3.0f * FC3 * SH3_B.V1[2];
		OutData.cBb.w = -FC2 * SH3_B.V1[3];

		OutData.cC.x = FC4 * SH3_R.V2;
		OutData.cC.y = FC4 * SH3_G.V2;
		OutData.cC.z = FC4 * SH3_B.V2;
		OutData.cC.w = 1.0f;

		Output[0] = OutData;
	}
}