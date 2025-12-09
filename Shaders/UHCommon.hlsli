#ifndef UHCOMMOM_H
#define UHCOMMOM_H

// UH common HLSL include, mainly for calculating functions
#define UH_FLOAT_EPSILON 1.19209e-07
#define UH_FLOAT_MAX 3.402823466e+38F
#define UH_PI 3.141592653589793f
// inverse PI = 1.0f / PI
#define UH_INVERSE_PI 0.3183098861837f
#define UH_RAD_TO_DEG 57.29577866f

struct UHRendererInstance
{
    // mesh index to lookup mesh data
    uint MeshIndex;
    // indice type
    uint IndiceType;
};

static const float4 GBoxOffset[8] =
{
    float4(-1.0f, -1.0f, 1.0f, 0.0f),
    float4(1.0f, -1.0f, 1.0f, 0.0f),
    float4(1.0f, 1.0f, 1.0f, 0.0f),
    float4(-1.0f, 1.0f, 1.0f, 0.0f),
    float4(-1.0f, -1.0f, -1.0f, 0.0f),
    float4(1.0f, -1.0f, -1.0f, 0.0f),
    float4(1.0f, 1.0f, -1.0f, 0.0f),
    float4(-1.0f, 1.0f, -1.0f, 0.0f)
};

float3 ComputeWorldPositionFromDeviceZ(float2 ScreenPos, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(ScreenPos, Depth, 1);
	NDCPos.xy *= GResolution.zw;
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? GViewProjInv_NonJittered : GViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

// input as uv
float3 ComputeWorldPositionFromDeviceZ_UV(float2 UV, float Depth, bool bNonJittered = false)
{
	// build NDC space position
	float4 NDCPos = float4(UV, Depth, 1);
	NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

	float4 WorldPos = mul(NDCPos, (bNonJittered) ? GViewProjInv_NonJittered : GViewProjInv);
	WorldPos.xyz /= WorldPos.w;

	return WorldPos.xyz;
}

float3 ComputeViewPositionFromDeviceZ(float2 ScreenPos, float Depth, bool bNonJittered = false)
{
	// build NDC space position
    float4 NDCPos = float4(ScreenPos, Depth, 1);
    NDCPos.xy *= GResolution.zw;
    NDCPos.xy = NDCPos.xy * 2.0f - 1.0f;

    float4 ViewPos = mul((bNonJittered) ? GProjInv_NonJittered : GProjInv, NDCPos);
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
	return normalize(mul(Normal, (float3x3)GWorldIT));
}

float3 LocalToWorldDir(float3 Dir)
{
	return normalize(mul(Dir, (float3x3)GWorld));
}

float3 WorldToViewPos(float3 Pos)
{
	// View matrix isn't transposed so the order is different than other matrix
	// inverse z at last
    return mul(GView, float4(Pos, 1.0f)).xyz * float3(1, 1, -1);
}

float3x3 CreateTBN(float3 InWorldNormal, float4 InTangent)
{
	float3 Tangent = LocalToWorldDir(InTangent.xyz);
	float3 Binormal = cross(InWorldNormal, Tangent) * InTangent.w;
    Tangent = cross(Binormal, InWorldNormal) * InTangent.w;

	float3x3 TBN = float3x3(Tangent, Binormal, InWorldNormal);
	return TBN;
}

float4x4 GetDistanceScaledJitterMatrix(float Dist)
{
	// calculate jitter offset based on the distance
	// so the distant objects have lower jitter, use a square curve
	float DistFactor = saturate(Dist * GJitterScaleFactor);
	DistFactor *= DistFactor;

	float OffsetX = lerp(GJitterOffsetX, GJitterOffsetX * GJitterScaleMin, DistFactor);
	float OffsetY = lerp(GJitterOffsetY, GJitterOffsetY * GJitterScaleMin, DistFactor);

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
// note that this function is used for VIEW space position instead of world position
// so there is a subtle difference than original implementation
bool SphereIntersectsFrustum(float4 Sphere, float4 Plane[6])
{
	UHUNROLL
    for (int Idx = 0; Idx < 6; Idx++)
    {
        float d = dot(Sphere.xyz, Plane[Idx].xyz) + Plane[Idx].w;
        if (d + Sphere.w < 0)
        {
            // it's outside one frustum plane already and is impossible to intersect
            return false;
        }
    }

    return true;
}

bool SphereIntersectsSphere(float4 Sphere1, float4 Sphere2)
{
	// check whether the square distance of s1 and s2 is less than the sum of squared radius
    // sqrt it when necessary (shouldn't need to)
    float3 SphereVec = Sphere1.xyz - Sphere2.xyz;
    float Dist = dot(SphereVec, SphereVec);

    float RadiusSum = Sphere1.w + Sphere2.w;
    RadiusSum *= RadiusSum;

    return Dist < RadiusSum;
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

bool IsUVInsideScreen(float2 UV, float Tolerance = 0.0f)
{
    return UV.x >= -Tolerance && UV.x <= (1.0f + Tolerance)
        && UV.y >= -Tolerance && UV.y <= (1.0f + Tolerance);
}

// https://en.wikipedia.org/wiki/Relative_luminance
float RGBToLuminance(float3 Color)
{
    return (0.2126f * Color.r + 0.7152f * Color.g + 0.0722f * Color.b);
}

// sphere-cone intersection, NOTE that it's done in light space.
// this means the cone position is the origin and input sphere needs to be transformed to light space first!
bool SphereIntersectsConeFrustum(float4 Sphere, float ConeHeight, float ConeAngle)
{
    // build corner
    // frustum corners - LB RB LT RT
    float3 Corners[4];
    float ConeRadius = tan(ConeAngle) * ConeHeight;
    float DistToConeCorner = sqrt(ConeRadius * ConeRadius + ConeRadius * ConeRadius);
    
    Corners[0] = float3(-DistToConeCorner, -DistToConeCorner, ConeHeight);
    Corners[1] = float3(DistToConeCorner, -DistToConeCorner, ConeHeight);
    Corners[2] = float3(-DistToConeCorner, DistToConeCorner, ConeHeight);
    Corners[3] = float3(DistToConeCorner, DistToConeCorner, ConeHeight);
    
    // buld planes
    // plane order: Left, Right, Bottom, Top, Near, Far
    float4 Plane[6];
    
    Plane[0].xyz = normalize(cross(Corners[2], Corners[0]));
    Plane[0].w = 0;

    Plane[1].xyz = -normalize(cross(Corners[3], Corners[1])); // flip so right plane point inside frustum
    Plane[1].w = 0;

    Plane[2].xyz = normalize(cross(Corners[0], Corners[1]));
    Plane[2].w = 0;

    Plane[3].xyz = -normalize(cross(Corners[2], Corners[3])); // flip so top plane point inside frustum
    Plane[3].w = 0;

    Plane[4].xyz = float3(0, 0, 1);
    Plane[4].w = 0;

    Plane[5].xyz = float3(0, 0, -1);
    Plane[5].w = ConeHeight;
    
    return SphereIntersectsFrustum(Sphere, Plane);
}

void BoxIntersectsPlane(float3 Center, float3 Extent, float4 Plane, out bool bIsOutside)
{
    float Dist = dot(float4(Center, 1.0f), Plane);
    
    // Project the axes of the box onto the normal of the plane.  Half the
    // length of the projection (sometime called the "radius") is equal to
    // h(u) * abs(n dot b(u))) + h(v) * abs(n dot b(v)) + h(w) * abs(n dot b(w))
    // where h(i) are extents of the box, n is the plane normal, and b(i) are the
    // axes of the box. In this case b(i) = [(1,0,0), (0,1,0), (0,0,1)].
    float Radius = dot(Extent, abs(Plane.xyz));
    
    // outside plane?
    bIsOutside = Dist + Radius < 0;
}

bool BoxIntersectsFrustum(float3 BoxMin, float3 BoxMax, float4 Plane[6])
{
    float3 Center = (BoxMin + BoxMax) * 0.5f;
    float3 Extent = (BoxMax - BoxMin) * 0.5f;
    
    // 6-plane test
    UHUNROLL

    for (int Idx = 0; Idx < 6; Idx++)
    {
        bool bIsOutside;
    
        BoxIntersectsPlane(Center, Extent, Plane[Idx], bIsOutside);
        if (bIsOutside)
        {
            // box is outside any plane already, no need to continue
            return false;
        }
    }
    
    // it's either contained by or intersects with the frustum
    return true;
}

bool BoxIntersectsConeFrustum(float3 BoxMin, float3 BoxMax, float ConeHeight, float ConeAngle)
{
    // build corner
    // frustum corners - LB RB LT RT
    float3 Corners[4];
    float ConeRadius = tan(ConeAngle) * ConeHeight;
    float DistToConeCorner = sqrt(ConeRadius * ConeRadius + ConeRadius * ConeRadius);
    
    Corners[0] = float3(-DistToConeCorner, -DistToConeCorner, ConeHeight);
    Corners[1] = float3(DistToConeCorner, -DistToConeCorner, ConeHeight);
    Corners[2] = float3(-DistToConeCorner, DistToConeCorner, ConeHeight);
    Corners[3] = float3(DistToConeCorner, DistToConeCorner, ConeHeight);
    
    // buld planes
    // plane order: Left, Right, Bottom, Top, Near, Far
    float4 Plane[6];
    
    Plane[0].xyz = normalize(cross(Corners[2], Corners[0]));
    Plane[0].w = 0;

    Plane[1].xyz = -normalize(cross(Corners[3], Corners[1])); // flip so right plane point inside frustum
    Plane[1].w = 0;

    Plane[2].xyz = normalize(cross(Corners[0], Corners[1]));
    Plane[2].w = 0;

    Plane[3].xyz = -normalize(cross(Corners[2], Corners[3])); // flip so top plane point inside frustum
    Plane[3].w = 0;

    Plane[4].xyz = float3(0, 0, 1);
    Plane[4].w = 0;

    Plane[5].xyz = float3(0, 0, -1);
    Plane[5].w = ConeHeight;
    
    return BoxIntersectsFrustum(BoxMin, BoxMax, Plane);
}

// https://en.wikipedia.org/wiki/Bh%C4%81skara_I%27s_sine_approximation_formula
float SineApprox(float Angle)
{
    Angle *= UH_RAD_TO_DEG;
    return 4.0f * Angle * (180.0f - Angle) / (40500.0f - Angle * (180.0f - Angle));
}

float2 SphereDirToUV(float3 SphereDir)
{
    float2 OutputUV;
    OutputUV.x = 0.5f + atan2(SphereDir.z, SphereDir.x) * 0.5f * UH_INVERSE_PI;
    OutputUV.y = 0.5f + asin(SphereDir.y) * UH_INVERSE_PI;
    
    return OutputUV;
}

#endif