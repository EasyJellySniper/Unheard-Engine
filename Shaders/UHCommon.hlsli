#ifndef UHCOMMOM_H
#define UHCOMMOM_H

// UH common HLSL include, mainly for calculating functions
#define UH_FLOAT_EPSILON 1.175494351e-38F
#define UH_FLOAT_MAX 3.402823466e+38F
#define UH_PI 3.141592653589793f

float3 ComputeWorldPositionFromDeviceZ(float2 ScreenPos, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(ScreenPos, Depth, 1);
	NDCPos.xy *= UHResolution.zw;
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? UHViewProjInv_NonJittered : UHViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

// input as uv
float3 ComputeWorldPositionFromDeviceZ_UV(float2 UV, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(UV, Depth, 1);
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? UHViewProjInv_NonJittered : UHViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

float3 ComputeViewPositionFromDeviceZ(float2 ScreenPos, float Depth, bool bNonJittered = false)
{
	// build NDC space position
    float4 NDCPos = float4(ScreenPos, Depth, 1);
    NDCPos.xy *= UHResolution.zw;
    NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

    float4 ViewPos = mul((bNonJittered) ? UHProjInv_NonJittered : UHProjInv, NDCPos);
    ViewPos.xyz /= ViewPos.w;

    return ViewPos.xyz * float3(1, 1, -1);
}

float3 ComputeSpecularColor(float3 Specular, float3 BaseColor, float Metallic)
{
	// lerp specular based on metallic, use 5% dieletric on specular for now
	return lerp(Specular * 0.05f, BaseColor, Metallic.rrr);
}

float3 LocalToWorldNormal(float3 Normal)
{
	// mul(IT_M, norm) => mul(norm, I_M) => {dot(norm, I_M.col0), dot(norm, I_M.col1), dot(norm, I_M.col2)}
	return normalize(mul(Normal, (float3x3)UHWorldIT));
}

float3 LocalToWorldDir(float3 Dir)
{
	return normalize(mul(Dir, (float3x3)UHWorld));
}

float3 WorldToViewPos(float3 Pos)
{
	// View matrix isn't transposed so the order is different than other matrix
	// inverse z at last
    return mul(UHView, float4(Pos, 1.0f)).xyz * float3(1, 1, -1);
}

float3x3 CreateTBN(float3 InWorldNormal, float4 InTangent)
{
	float3 Tangent = LocalToWorldDir(InTangent.xyz);
	float3 Binormal = cross(InWorldNormal, Tangent) * InTangent.w;

	float3x3 TBN = float3x3(Tangent, Binormal, InWorldNormal);
	return TBN;
}

float4x4 GetDistanceScaledJitterMatrix(float Dist)
{
	// calculate jitter offset based on the distance
	// so the distant objects have lower jitter, use a square curve
	float DistFactor = saturate(Dist * JitterScaleFactor);
	DistFactor *= DistFactor;

	float OffsetX = lerp(JitterOffsetX, JitterOffsetX * JitterScaleMin, DistFactor);
	float OffsetY = lerp(JitterOffsetY, JitterOffsetY * JitterScaleMin, DistFactor);

	float4x4 JitterMatrix = 
	{
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		OffsetX,OffsetY,0,1
	};

	return JitterMatrix;
}

// box-sphere intersection from Graphics Gems 2 by Jim Arvo
bool BoxIntersectsSphere(float3 BoxMin, float3 BoxMax, float3 SphereCenter, float SphereRadius)
{
    float RadiusSqr = SphereRadius * SphereRadius;
    float MinDistanceSqr = 0;
	
	UHBRANCH
    if (SphereCenter.x < BoxMin.x)
    {
        MinDistanceSqr += (SphereCenter.x - BoxMin.x) * (SphereCenter.x - BoxMin.x);
    }
	else if (SphereCenter.x > BoxMax.x)
    {
        MinDistanceSqr += (SphereCenter.x - BoxMax.x) * (SphereCenter.x - BoxMax.x);
    }
	
    UHBRANCH
    if (SphereCenter.y < BoxMin.y)
    {
        MinDistanceSqr += (SphereCenter.y - BoxMin.y) * (SphereCenter.y - BoxMin.y);
    }
    else if (SphereCenter.y > BoxMax.y)
    {
        MinDistanceSqr += (SphereCenter.y - BoxMax.y) * (SphereCenter.y - BoxMax.y);
    }
	
    UHBRANCH
    if (SphereCenter.z < BoxMin.z)
    {
        MinDistanceSqr += (SphereCenter.z - BoxMin.z) * (SphereCenter.z - BoxMin.z);
    }
    else if (SphereCenter.z > BoxMax.z)
    {
        MinDistanceSqr += (SphereCenter.z - BoxMax.z) * (SphereCenter.z - BoxMax.z);
    }
	
    return MinDistanceSqr <= RadiusSqr;
}

// sphere-frustum intersection
// Source: Real-time collision detection, Christer Ericson (2005)
bool SphereIntersectsFrustum(float4 Sphere, float4 Plane[6])
{
    bool Result = true;

	UHUNROLL
    for (int Idx = 0; Idx < 6; Idx++)
    {
        float d = dot(Sphere.xyz, Plane[Idx].xyz) + Plane[Idx].w;
        if (d < -Sphere.w)
        {
            Result = false;
        }
    }

    return Result;
}

// 2D coordinate to random hash function
// the source of the magic number is from an old paper "On generating random numbers, with help of y= [(a+x)sin(bx)] mod 1"
// , W.J.J. Rey, 22nd European Meeting of Statisticians and the 7th Vilnius Conference on Probability Theory and Mathematical Statistics, August 1998
// https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
float CoordinateToHash(float2 InCoord)
{
    float T = 12.9898f * InCoord.x + 78.233f * InCoord.y;
    return frac(sin(T) * 43758.0f);
}

float3 EncodeNormal(float3 N)
{
    return N * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 N, bool bIsBC5 = false)
{
    // BC5 stores 2 channels only, so calculating z component by xy
    if (bIsBC5)
    {
        N.xy = N.xy * 2.0f - 1.0f;
        N.z = sqrt(saturate(1.0f - (N.x * N.x + N.y * N.y)));
        return normalize(N);
    }

    return normalize(N * 2.0f - 1.0f);
}

float GetAttenuationNoise(float2 InCoord)
{
    float Offset = 0.03f;
    return lerp(-Offset, Offset, CoordinateToHash(InCoord));
}

float3 ConvertUVToSpherePos(float2 UV)
{
	// convert to sphere coordinate based on https://en.wikipedia.org/wiki/Spherical_coordinate_system definition
	// x = sin(theta * v) * cos(phi * u)
	// y = sin(theta * v) * sin(phi * u)
	// z = cos(theta * v)
	// theta = [0, PI], and phi = [0, 2PI]
	float Theta = UH_PI;
	float Phi = UH_PI * 2.0f;

	// assume input UV is [0,1], simply convert y in [-1,1] as cos theta and calculate sin theta, which can reduce the sin/cos call below
	float CosTheta = 1.0f - 2.0f * UV.y;
	float SinTheta = sqrt(1.0f - CosTheta * CosTheta);

	float3 Pos;
	Pos.x = SinTheta * cos(UV.x * Phi);
	Pos.y = SinTheta * sin(UV.x * Phi);
	Pos.z = CosTheta;

	// flip the result to matach hardward implementation
	return float3(Pos.y, Pos.z, -Pos.x);
}

#endif