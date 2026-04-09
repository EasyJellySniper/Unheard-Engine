#include "Math.h"
#include "Runtime/CoreGlobals.h"
#include <cmath>

namespace UHMathHelpers
{
    bool IsValidVector(UHVector3 InVector)
    {
        return !glm::any(glm::isnan(InVector));
    }

    bool IsVectorNearlyZero(UHVector3 InVector)
    {
        static UHVector3 V2(GEpsilon, GEpsilon, GEpsilon);
        return glm::all(glm::lessThanEqual(InVector, V2));
    }

    bool IsVectorNearlyZero(UHVector4 InVector)
    {
        static UHVector4 V2(GEpsilon, GEpsilon, GEpsilon, GEpsilon);
        return glm::all(glm::lessThanEqual(InVector, V2));
    }

    bool IsVectorEqual(UHVector2 InA, UHVector2 InB)
    {
        return glm::all(glm::equal(InA, InB));
    }

    bool IsVectorEqual(UHVector3 InA, UHVector3 InB)
    {
        return glm::all(glm::equal(InA, InB));
    }

    bool IsVectorEqual(UHVector4 InA, UHVector4 InB)
    {
        return glm::all(glm::equal(InA, InB));
    }

    UHMatrix4x4 Identity4x4()
    {
        static UHMatrix4x4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    }

    UHGPUMatrix3x4 MatrixTo3x4(UHMatrix4x4 InMatrix)
    {
        UHGPUMatrix3x4 OutMatrix;

        // ignore the 4th row in 4X4 matrix, this also implicitly does a transpose
        for (int32_t I = 0; I < 3; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                OutMatrix.M[I][J] = InMatrix[I][J];
            }
        }

        return OutMatrix;
    }

    float RandFloat()
    {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }

    // matrix to pitch yaw roll, use for display only!
    // https://en.wikipedia.org/wiki/Euler_angles#Conversion_to_other_orientation_representations
    // the Y1Z2X3 case
    void MatrixToPitchYawRoll(UHMatrix4x4 InMat, float& Pitch, float& Yaw, float& Roll)
    {
        UHMatrix4x4 M = UHMatrixTranspose(InMat);

        if (M[0][0] == 1.0f)
        {
            Yaw = std::atan2f(M[0][2], M[2][3]);
            Pitch = 0;
            Roll = 0;

        }
        else if (M[0][0] == -1.0f)
        {
            Yaw = std::atan2f(M[0][2], M[2][3]);
            Pitch = 0;
            Roll = 0;
        }
        else
        {
            Yaw = std::atan2(-M[2][0], M[0][0]);
            Roll = std::asin(M[1][0]);
            Pitch = std::atan2(-M[1][2], M[1][1]);
        }

        // to degrees
        Pitch = ToDegrees(Pitch);
        Yaw = ToDegrees(Yaw);
        Roll = ToDegrees(Roll);
    }

    UHVector3 MinVector(const UHVector3& InVector, const UHVector3& InVector2)
    {
        return glm::min(InVector, InVector2);
    }

    UHVector3 MaxVector(const UHVector3& InVector, const UHVector3& InVector2)
    {
        return glm::max(InVector, InVector2);
    }

    // halton sequence generation, reference: https://en.wikipedia.org/wiki/Halton_sequence
    // use prime number for input base
    float Halton(int32_t Index, int32_t Base)
    {
        float Result = 0.0f;
        float Fraction = 1.0f;
        float InvBase = 1.0f / static_cast<float>(Base);

        while (Index > 0)
        {
            Fraction *= InvBase;
            Result += Fraction * (Index % Base);
            Index /= Base;
        }

        return Result;
    }

    float Lerp(const float& InVal1, const float& InVal2, const float& T)
    {
        return InVal1 * (1.0f - T) + InVal2 * T;
    }

    UHVector3 LerpVector(const UHVector3& InVector, const UHVector3& InVector2, const float& T)
    {
        return glm::mix(InVector, InVector2, T);
    }

    float VectorDistanceSqr(const UHVector3& InVector, const UHVector3& InVector2)
    {
        UHVector3 Diff = InVector - InVector2;
        return glm::dot(Diff, Diff);
    }

    // the bithacks trick
    // https://graphics.stanford.edu/%7Eseander/bithacks.html#RoundUpPowerOf2
    float RoundUpToClosestPowerOfTwo(float InVal)
    {
        uint32_t V = static_cast<uint32_t>(InVal);

        V--;
        V |= V >> 1;
        V |= V >> 2;
        V |= V >> 4;
        V |= V >> 8;
        V |= V >> 16;
        V++;

        return static_cast<float>(V);
    }

    float RoundUpToMultiple(float InVal, float InMultiple)
    {
        return std::roundf(InVal / InMultiple) * InMultiple;
    }

    // matrix functions
    UHMatrix4x4 UHMatrixTranspose(UHMatrix4x4 M)
    {
        return glm::transpose(M);
    }
    
    UHMatrix4x4 UHMatrixTranslation(UHVector3 Translation)
    {
        return glm::translate(Identity4x4(), Translation);
    }

    UHMatrix4x4 UHMatrixScaling(UHVector3 Scale)
    {
        return glm::scale(Identity4x4(), Scale);
    }

    UHMatrix4x4 UHMatrixPerspectiveFovLH(float FovAngleY, float Width, float Height, float NearZ, float FarZ)
    {
        return glm::perspectiveFovLH(FovAngleY, Width, Height, NearZ, FarZ);
    }

    UHMatrix4x4 UHMatrixPerspectiveFovRH(float FovAngleY, float Width, float Height, float NearZ, float FarZ)
    {
        return glm::perspectiveFovRH(FovAngleY, Width, Height, NearZ, FarZ);
    }

    UHMatrix4x4 UHMatrixLookToRH(UHVector3 Position, UHVector3 Forward, UHVector3 Up)
    {
        return glm::lookAtRH(Position, Position + Forward, Up);
    }

    UHMatrix4x4 UHMatrixInverse(UHMatrix4x4 M)
    {
        return glm::inverse(M);
    }

    UHMatrix4x4 UHMatrixRotationRollPitchYaw(float Pitch, float Yaw, float Roll) noexcept
    {
        UHMatrix4x4 Ry = glm::rotate(UHMatrix4x4(1.0f), Yaw, UHVector3(0, 1, 0));
        UHMatrix4x4 Rx = glm::rotate(UHMatrix4x4(1.0f), Pitch, UHVector3(1, 0, 0));
        UHMatrix4x4 Rz = glm::rotate(UHMatrix4x4(1.0f), Roll, UHVector3(0, 0, 1));

        return Ry * Rx * Rz;
    }

    UHMatrix4x4 UHMatrixRotationAxis(UHVector3 Axis, float Angle)
    {
        return glm::rotate(UHMatrix4x4(1.0f), Angle, Axis);
    }

    void CreateFromVector(UHMatrix4x4& OutM, const UHVector4& R0, const UHVector4& R1, const UHVector4& R2, const UHVector4& R3) noexcept
    {
        OutM[0][0] = R0.x;
        OutM[0][1] = R0.y;
        OutM[0][2] = R0.z;
        OutM[0][3] = R0.w;

        OutM[1][0] = R1.x;
        OutM[1][1] = R1.y;
        OutM[1][2] = R1.z;
        OutM[1][3] = R1.w;

        OutM[2][0] = R2.x;
        OutM[2][1] = R2.y;
        OutM[2][2] = R2.z;
        OutM[2][3] = R2.w;

        OutM[3][0] = R3.x;
        OutM[3][1] = R3.y;
        OutM[3][2] = R3.z;
        OutM[3][3] = R3.w;
    }

    // vector functions
    UHVector4 UHVector3Transform(UHVector3 V, UHMatrix4x4 M)
    {
        return M * UHVector4(V.x, V.y, V.z, 1.0f);
    }

    UHVector3 UHVector3TransformNormal(UHVector3 V, UHMatrix4x4 M)
    {
        return glm::mat3(M) * V;
    }

    bool UHVector3InBound(UHVector3 Vec, UHVector3 Bounds)
    {
        // is the vector in extent? Assume the inputs are in local (relative) space already
        return Vec.x <= Bounds.x && Vec.x >= -Bounds.x
            && Vec.y <= Bounds.y && Vec.y >= -Bounds.y
            && Vec.z <= Bounds.z && Vec.z >= -Bounds.z;
    }

    UHVector3 UHVector3Round(UHVector3 V)
    {
        return glm::round(V);
    }

    UHVector4 UHVector3Normalize(UHVector4 V)
    {
        // normalize 3d part of a vector4, the w component remains unchanged
        UHVector3 Result = UHVector3Normalize(UHVector3(V));
        return UHVector4(Result.x, Result.y, Result.z, V.w);
    }

    UHVector3 UHVector3Normalize(UHVector3 V)
    {
        return glm::normalize(V);
    }

    UHVector4 UHQuaternionRotationMatrix(UHMatrix4x4 M)
    {
        glm::quat Q = glm::quat_cast(M);
        return UHVector4(Q.x, Q.y, Q.z, Q.w);
    }

    UHVector4 UHQuaternionMultiply(UHVector4 Q1, UHVector4 Q2)
    {
        glm::quat GQ1(Q1.w, Q1.x, Q1.y, Q1.z);
        glm::quat GQ2(Q2.w, Q2.x, Q2.y, Q2.z);
        GQ1 *= GQ2;

        return UHVector4(GQ1.x, GQ1.y, GQ1.z, GQ1.w);
    }

    float UHVector3Dot(UHVector4 V1, UHVector4 V2)
    {
        UHVector3 GV1(V1);
        UHVector3 GV2(V2);

        return glm::dot(GV1, GV2);
    }

    float UHVector3Dot(UHVector3 V1, UHVector3 V2)
    {
        return glm::dot(V1, V2);
    }

    UHVector4 UHVector4Transform(UHVector4 V, UHMatrix4x4 M)
    {
        return M * V;
    }

    UHVector4 UHVector4Multiply(UHVector4 V1, UHVector4 V2)
    {
        return V1 * V2;
    }

    UHVector4 UHVector4SplatZ(UHVector4 V)
    {
        return glm::splatZ(V);
    }

    UHVector4 UHVector4SplatW(UHVector4 V)
    {
        return glm::splatW(V);
    }

    UHVector4 UHVector4Reciprocal(UHVector4 V)
    {
        return 1.0f / V;
    }

    // Transform a vector using a rotation expressed as a unit quaternion
    UHVector3 UHVector3Rotate(UHVector4 V, UHVector4 Q)
    {
        UHVector3 GV(V);
        glm::quat GQ(Q.w, Q.x, Q.y, Q.z);

        return GQ * GV;
    }

    float UHVector4Dot(UHVector4 V1, UHVector4 V2)
    {
        return glm::dot(V1, V2);
    }

    UHVector3 UHVector3Abs(UHVector3 V)
    {
        return glm::abs(V);
    }

    // plane functions
    UHVector4 UHPlaneTransform(UHVector4 Plane, UHVector4 Rotation, UHVector3 Translation)
    {
        UHVector3 VNormal = UHVector3Rotate(Plane, Rotation);
        float Dist = Plane.w - UHVector3Dot(VNormal, Translation);

        return UHVector4(VNormal.x, VNormal.y, VNormal.z, Dist);
    }

    UHVector4 UHPlaneNormalize(UHVector4 Plane)
    {
        float Length = glm::length(UHVector3(Plane));

        UHVector4 Result = Plane;
        Result /= Length;
        return Result;
    }

    // intersection
    void FastIntersectAxisAlignedBoxPlane(UHVector4 Center, UHVector3 Extents, UHVector4 Plane,
        bool& Outside, bool& Inside) noexcept
    {
        float Dist = UHVector4Dot(Center, Plane);

        // Project the axes of the box onto the normal of the plane.  Half the
        // length of the projection (sometime called the "radius") is equal to
        // h(u) * abs(n dot b(u))) + h(v) * abs(n dot b(v)) + h(w) * abs(n dot b(w))
        // where h(i) are extents of the box, n is the plane normal, and b(i) are the
        // axes of the box. In this case b(i) = [(1,0,0), (0,1,0), (0,0,1)].
        float Radius = UHVector3Dot(Extents, UHVector3Abs(UHVector3(Plane)));

        Outside = Dist > Radius;
        Inside = Dist < -Radius;
    }

    // degrees
    float ToRadians(float InDegrees)
    {
        return InDegrees * (G_PI / 180.0f);
    }

    float ToDegrees(float InRadians)
    {
        return InRadians * (180.0f / G_PI);
    }
}

// ----------------------------------------------------------- UHBoundingBox
bool UHBoundingBox::Contains(UHVector3 Point) const noexcept
{
    return UHMathHelpers::UHVector3InBound(Point - Center, Extents);
}

bool UHBoundingBox::ContainedBy(UHVector4 Plane0, UHVector4 Plane1, UHVector4 Plane2,
    UHVector4 Plane3, UHVector4 Plane4, UHVector4 Plane5) const noexcept
{
    UHVector4 CenterWithW(Center.x, Center.y, Center.z, 1.0f);

    bool bOutside, bInside;
    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane0, bOutside, bInside);

    bool bAnyOutside = bOutside;
    bool bAllInside = bInside;

    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane1, bOutside, bInside);
    bAnyOutside |= bOutside;
    bAllInside &= bInside;

    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane2, bOutside, bInside);
    bAnyOutside |= bOutside;
    bAllInside &= bInside;

    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane3, bOutside, bInside);
    bAnyOutside |= bOutside;
    bAllInside &= bInside;

    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane4, bOutside, bInside);
    bAnyOutside |= bOutside;
    bAllInside &= bInside;

    UHMathHelpers::FastIntersectAxisAlignedBoxPlane(CenterWithW, Extents, Plane5, bOutside, bInside);
    bAnyOutside |= bOutside;
    bAllInside &= bInside;

    // If the box is outside any plane it is outside.
    if (bAnyOutside)
    {
        return false;
    }

    // If the box is inside all planes it is inside.
    if (bAllInside)
    {
        return true;
    }

    // intersected, still considered as contained
    return true;
}

void UHBoundingBox::Transform(UHBoundingBox& Out, UHMatrix4x4 M) const noexcept
{
    // Load center and extents.
    UHVector3 Offset(GBoxOffset[0]);
    UHVector3 Corner = Extents * Offset + Center;

    UHVector4 TransformedCorner = UHMathHelpers::UHVector3Transform(Corner, M);
    Corner = UHVector3(TransformedCorner);

    UHVector3 Min, Max;
    Min = Max = Corner;

    for (size_t I = 1; I < CORNER_COUNT; I++)
    {
        Offset = UHVector3(GBoxOffset[I]);
        Corner = Extents * Offset + Center;

        TransformedCorner = UHMathHelpers::UHVector3Transform(Corner, M);
        Corner = UHVector3(TransformedCorner);

        Min = glm::min(Min, Corner);
        Max = glm::max(Max, Corner);
    }

    Out.Center = (Min + Max) * 0.5f;
    Out.Extents = (Max - Min) * 0.5f;
}

void UHBoundingBox::GetCorners(UHVector3* Corners) const noexcept
{
    assert(Corners != nullptr);

    for (size_t I = 0; I < CORNER_COUNT; I++)
    {
        UHVector3 Offset(GBoxOffset[I]);
        Corners[I] = Extents * Offset + Center;
    }
}

void UHBoundingBox::CreateMerged(UHBoundingBox& Out, const UHBoundingBox& b1, const UHBoundingBox& b2) noexcept
{
    UHVector3 Min = glm::min(b1.Center - b1.Extents, b2.Center - b2.Extents);
    UHVector3 Max = glm::max(b1.Center + b1.Extents, b2.Center + b2.Extents);

    Out.Center = (Min + Max) * 0.5f;
    Out.Extents = (Max - Min) * 0.5f;
}

// ----------------------------------------------------------- UHBoundingFrustum
void UHBoundingFrustum::Transform(UHBoundingFrustum& Out, UHMatrix4x4 M) const noexcept
{
    // transform the center
    UHVector4 TransformedOrigin = UHMathHelpers::UHVector3Transform(Origin, M);

    // Composite the frustum rotation and the transform rotation
    UHVector4 R0(M[0][0], M[0][1], M[0][2], M[0][3]);
    UHVector4 R1(M[1][0], M[1][1], M[1][2], M[1][3]);
    UHVector4 R2(M[2][0], M[2][1], M[2][2], M[2][3]);
    UHVector4 R3(0, 0, 0, 1);
    R0 = UHMathHelpers::UHVector3Normalize(R0);
    R1 = UHMathHelpers::UHVector3Normalize(R1);
    R2 = UHMathHelpers::UHVector3Normalize(R2);

    UHMatrix4x4 NormalizedM;
    UHMathHelpers::CreateFromVector(NormalizedM, R0, R1, R2, R3);
    UHVector4 Rotation = UHMathHelpers::UHQuaternionRotationMatrix(NormalizedM);

    // output transformed origin and orientation
    Out.Origin = TransformedOrigin;
    Out.Orientation = UHMathHelpers::UHQuaternionMultiply(Orientation, Rotation);

    // slope remains the same
    Out.RightSlope = RightSlope;
    Out.LeftSlope = LeftSlope;
    Out.TopSlope = TopSlope;
    Out.BottomSlope = BottomSlope;

    // Scale the near and far distances
    float DotX = UHMathHelpers::UHVector3Dot(R0, R0);
    float DotY = UHMathHelpers::UHVector3Dot(R1, R1);
    float DotZ = UHMathHelpers::UHVector3Dot(R2, R2);
    float MaxD = std::max(DotX, std::max(DotY, DotZ));
    float Scale = std::sqrtf(MaxD);

    Out.Near = Near * Scale;
    Out.Far = Far * Scale;
}

bool UHBoundingFrustum::Contains(const UHBoundingBox& Box) const noexcept
{
    // Create 6 planes
    UHVector4 NearPlane(0.0f, 0.0f, -1.0f, Near);
    NearPlane = UHMathHelpers::UHPlaneTransform(NearPlane, Orientation, Origin);
    NearPlane = UHMathHelpers::UHPlaneNormalize(NearPlane);

    UHVector4 FarPlane(0.0f, 0.0f, 1.0f, -Far);
    FarPlane = UHMathHelpers::UHPlaneTransform(FarPlane, Orientation, Origin);
    FarPlane = UHMathHelpers::UHPlaneNormalize(FarPlane);

    UHVector4 RightPlane(1.0f, 0.0f, -RightSlope, 0.0f);
    RightPlane = UHMathHelpers::UHPlaneTransform(RightPlane, Orientation, Origin);
    RightPlane = UHMathHelpers::UHPlaneNormalize(RightPlane);

    UHVector4 LeftPlane(-1.0f, 0.0f, LeftSlope, 0.0f);
    LeftPlane = UHMathHelpers::UHPlaneTransform(LeftPlane, Orientation, Origin);
    LeftPlane = UHMathHelpers::UHPlaneNormalize(LeftPlane);

    UHVector4 TopPlane(0.0f, 1.0f, -TopSlope, 0.0f);
    TopPlane = UHMathHelpers::UHPlaneTransform(TopPlane, Orientation, Origin);
    TopPlane = UHMathHelpers::UHPlaneNormalize(TopPlane);

    UHVector4 BottomPlane(0.0f, -1.0f, BottomSlope, 0.0f);
    BottomPlane = UHMathHelpers::UHPlaneTransform(BottomPlane, Orientation, Origin);
    BottomPlane = UHMathHelpers::UHPlaneNormalize(BottomPlane);
    
    return Box.ContainedBy(NearPlane, FarPlane, RightPlane, LeftPlane, TopPlane, BottomPlane);
}

//-----------------------------------------------------------------------------
// Build a frustum from a persepective projection matrix.  The matrix may only
// contain a projection; any rotation, translation or scale will cause the
// constructed frustum to be incorrect.
//-----------------------------------------------------------------------------
void UHBoundingFrustum::CreateFromMatrix(UHBoundingFrustum& Out, UHMatrix4x4 Projection, bool rhcoords) noexcept
{
    // Corners of the projection frustum in NDC space.
    static UHVector4 NDCPoints[6] =
    {
        UHVector4{  1.0f,  0.0f, 1.0f, 1.0f },   // right (at far plane)
        UHVector4{ -1.0f,  0.0f, 1.0f, 1.0f },   // left
        UHVector4{  0.0f,  1.0f, 1.0f, 1.0f },   // top
        UHVector4{  0.0f, -1.0f, 1.0f, 1.0f },   // bottom

        UHVector4{ 0.0f, 0.0f, 0.0f, 1.0f },     // near
        UHVector4{ 0.0f, 0.0f, 1.0f, 1.0f }      // far
    };

    UHMatrix4x4 InvProj = UHMathHelpers::UHMatrixInverse(Projection);

    // Compute the frustum corners in world space.
    UHVector4 Points[6];
    for (size_t I = 0; I < 6; I++)
    {
        // Transform point.
        Points[I] = UHMathHelpers::UHVector4Transform(NDCPoints[I], InvProj);
    }

    // init origin and orientation
    Out.Origin = UHVector3(0.0f, 0.0f, 0.0f);
    Out.Orientation = UHVector4(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Compute the slopes.
    Points[0] = UHMathHelpers::UHVector4Multiply(Points[0], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatZ(Points[0])));
    Points[1] = UHMathHelpers::UHVector4Multiply(Points[1], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatZ(Points[1])));
    Points[2] = UHMathHelpers::UHVector4Multiply(Points[2], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatZ(Points[2])));
    Points[3] = UHMathHelpers::UHVector4Multiply(Points[3], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatZ(Points[3])));

    Out.RightSlope = Points[0].x;
    Out.LeftSlope = Points[1].x;
    Out.TopSlope = Points[2].y;
    Out.BottomSlope = Points[3].y;

    // Compute near and far.
    Points[4] = UHMathHelpers::UHVector4Multiply(Points[4], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatW(Points[4])));
    Points[5] = UHMathHelpers::UHVector4Multiply(Points[5], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatW(Points[5])));

    if (rhcoords)
    {
        Out.Near = Points[5].z;
        Out.Far = Points[4].z;
    }
    else
    {
        Out.Near = Points[4].z;
        Out.Far = Points[5].z;
    }
}