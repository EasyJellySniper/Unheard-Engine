#ifndef UH_SHCOMMOM_H
#define UH_SHCOMMOM_H

// SH9 implementation based on the "Stupid Spherical Harmonics (SH) Tricks" paper
struct UHSphericalHarmonicData
{
	float4 cAr;
	float4 cAg;
	float4 cAb;
	float4 cBr;
	float4 cBg;
	float4 cBb;
	float4 cC;
};

struct UHSphericalHarmonicVector3
{
	float4 V0;
	float4 V1;
	float V2;
};

#ifndef SH9_BIND
#define SH9_BIND t0
#endif
StructuredBuffer<UHSphericalHarmonicData> SH9Data : register(SH9_BIND);

UHSphericalHarmonicVector3 SHBasis3(float3 InputVector)
{
	// https://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics
	// Here is to collect order 0 to 2 parameters, 9 parameters in total
	UHSphericalHarmonicVector3 Result;
	Result.V0.x = 0.282095f;
	Result.V0.y = -0.488603f * InputVector.y;
	Result.V0.z = 0.488603f * InputVector.z;
	Result.V0.w = -0.488603f * InputVector.x;

	float3 VectorSquared = InputVector * InputVector;
	Result.V1.x = 1.092548f * InputVector.x * InputVector.y;
	Result.V1.y = -1.092548f * InputVector.y * InputVector.z;
	Result.V1.z = 0.315392f * (3.0f * VectorSquared.z - 1.0f);
	Result.V1.w = -1.092548f * InputVector.x * InputVector.z;
	Result.V2 = 0.546274f * (VectorSquared.x - VectorSquared.y);

	return Result;
}

void MultiplySH3(inout UHSphericalHarmonicVector3 InVector, float Color)
{
	InVector.V0 = InVector.V0 * Color;
	InVector.V1 = InVector.V1 * Color;
	InVector.V2 = InVector.V2 * Color;
}

void AddSH3(inout UHSphericalHarmonicVector3 A, UHSphericalHarmonicVector3 B)
{
	A.V0 = A.V0 + B.V0;
	A.V1 = A.V1 + B.V1;
	A.V2 = A.V2 + B.V2;
}

float3 ShadeSH9(float3 Diffuse, float4 Normal, float Occlusion)
{
	float3 X1, X2, X3;
	// Linear + constant polynomial terms
	X1.r = dot(SH9Data[0].cAr, Normal);
	X1.g = dot(SH9Data[0].cAg, Normal);
	X1.b = dot(SH9Data[0].cAb, Normal);

	// 4 of the quadratic polynomials
	float4 VB = Normal.xyzz * Normal.yzzx;
	X2.r = dot(SH9Data[0].cBr, VB);
	X2.g = dot(SH9Data[0].cBg, VB);
	X2.b = dot(SH9Data[0].cBb, VB);

	// Final quadratic polynomial
	float VC = Normal.x * Normal.x - Normal.y * Normal.y;
	X3 = SH9Data[0].cC.rgb * VC;

	float3 IndirectDiffuse = (X1 + X2 + X3) * UHAmbientSky + lerp(UHAmbientGround, 0, Normal.y * 0.5f + 0.5f);
	return Diffuse * IndirectDiffuse * Occlusion;
}

#endif