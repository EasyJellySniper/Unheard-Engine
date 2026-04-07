#include "Math.h"
#include "Runtime/CoreGlobals.h"
#include <cmath>

// GLM math library, setup for graphics API use and SIMD optimizations
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

glm::mat4x4 TempUHToGLMMat(UHMatrix4x4 InMat)
{
    glm::mat4x4 OutM;
    for (int32_t I = 0; I < 4; I++)
    {
        for (int32_t J = 0; J < 4; J++)
        {
            OutM[I][J] = InMat.M[I][J];
        }
    }

    return OutM;
}

UHMatrix4x4 TempGLMToUHMat(glm::mat4x4 InMat)
{
    UHMatrix4x4 OutM;
    for (int32_t I = 0; I < 4; I++)
    {
        for (int32_t J = 0; J < 4; J++)
        {
            OutM.M[I][J] = InMat[I][J];
        }
    }

    return OutM;
}

namespace UHMathHelpers
{
    bool IsValidVector(UHVector3 InVector)
    {
        glm::vec3 Vec(InVector.X, InVector.Y, InVector.Z);
        glm::bvec3 Result = glm::isnan(Vec);

        return !Result.x && !Result.y && !Result.z;
    }

    bool IsVectorNearlyZero(UHVector3 InVector)
    {
        glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
        glm::vec3 V2(GEpsilon, GEpsilon, GEpsilon);
        glm::bvec3 Result = glm::lessThanEqual(V1, V2);

        return glm::all(Result);
    }

    bool IsVectorNearlyZero(UHVector4 InVector)
    {
        glm::vec4 V1(InVector.X, InVector.Y, InVector.Z, InVector.W);
        glm::vec4 V2(GEpsilon, GEpsilon, GEpsilon, GEpsilon);
        glm::bvec4 Result = glm::lessThanEqual(V1, V2);

        return glm::all(Result);
    }

    bool IsVectorEqual(UHVector2 InA, UHVector2 InB)
    {
        glm::vec2 V1(InA.X, InA.Y);
        glm::vec2 V2(InB.X, InB.Y);
        glm::bvec2 Result = glm::equal(V1, V2);

        return glm::all(Result);
    }

    bool IsVectorEqual(UHVector3 InA, UHVector3 InB)
    {
        glm::vec3 V1(InA.X, InA.Y, InA.Z);
        glm::vec3 V2(InB.X, InB.Y, InB.Z);
        glm::bvec3 Result = glm::equal(V1, V2);

        return glm::all(Result);
    }

    bool IsVectorEqual(UHVector4 InA, UHVector4 InB)
    {
        glm::vec4 V1(InA.X, InA.Y, InA.Z, InA.W);
        glm::vec4 V2(InB.X, InB.Y, InB.Z, InB.W);
        glm::bvec4 Result = glm::equal(V1, V2);

        return glm::all(Result);
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

    UHMatrix3x4 MatrixTo3x4(UHMatrix4x4 InMatrix)
    {
        UHMatrix3x4 OutMatrix = UHMatrix3x4();

        // just ignore the 4th row in 4X4 matrix
        for (int32_t I = 0; I < 3; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                OutMatrix.M[I][J] = InMatrix.M[I][J];
            }
        }

        return OutMatrix;
    }

    UHMatrix3x4 Identity3x4()
    {
        static UHMatrix3x4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f);

        return I;
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
        glm::mat4x4 M = TempUHToGLMMat(InMat);
        M = glm::transpose(M);

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
        glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
        glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
        V1 = glm::min(V1, V2);

        return UHVector3(V1.x, V1.y, V1.z);
    }

    UHVector3 MaxVector(const UHVector3& InVector, const UHVector3& InVector2)
    {
        glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
        glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
        V1 = glm::max(V1, V2);

        return UHVector3(V1.x, V1.y, V1.z);
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
        glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
        glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
        V1 = glm::mix(V1, V2, T);

        return UHVector3(V1.x, V1.y, V1.z);
    }

    float VectorDistanceSqr(const UHVector3& InVector, const UHVector3& InVector2)
    {
        glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
        glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
        glm::vec3 Diff = V1 - V2;

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
        glm::mat4x4 FM = TempUHToGLMMat(M);
        FM = glm::transpose(FM);

        return TempGLMToUHMat(FM);
    }
    
    UHMatrix4x4 UHMatrixTranslation(UHVector3 V)
    {
        glm::vec3 Translation(V.X, V.Y, V.Z);
        glm::mat4x4 M = glm::identity<glm::mat4x4>();
        M = glm::translate(M, Translation);

        return TempGLMToUHMat(M);
    }

    UHMatrix4x4 UHMatrixScaling(UHVector3 V)
    {
        glm::vec3 Scale(V.X, V.Y, V.Z);
        glm::mat4x4 M = glm::identity<glm::mat4x4>();
        M = glm::scale(M, Scale);

        return TempGLMToUHMat(M);
    }

    UHMatrix4x4 UHMatrixPerspectiveFovLH(float FovAngleY, float Width, float Height, float NearZ, float FarZ)
    {
        glm::mat4x4 M = glm::perspectiveFovLH(FovAngleY, Width, Height, NearZ, FarZ);
        return TempGLMToUHMat(M);
    }

    UHMatrix4x4 UHMatrixPerspectiveFovRH(float FovAngleY, float Width, float Height, float NearZ, float FarZ)
    {
        glm::mat4x4 M = glm::perspectiveFovRH(FovAngleY, Width, Height, NearZ, FarZ);
        return TempGLMToUHMat(M);
    }

    UHMatrix4x4 UHMatrixLookToRH(UHVector3 Position, UHVector3 Forward, UHVector3 Up)
    {
        glm::vec3 Eye(Position.X, Position.Y, Position.Z);
        glm::vec3 Center = Eye + glm::vec3(Forward.X, Forward.Y, Forward.Z);
        glm::vec3 U(Up.X, Up.Y, Up.Z);
        glm::mat4x4 M = glm::lookAtRH(Eye, Center, U);

        return TempGLMToUHMat(M);
    }

    UHMatrix4x4 UHMatrixInverse(UHMatrix4x4 M)
    {
        glm::mat4x4 FM = glm::inverse(TempUHToGLMMat(M));
        return TempGLMToUHMat(FM);
    }

    UHMatrix4x4 UHMatrixRotationRollPitchYaw(float Pitch, float Yaw, float Roll) noexcept
    {
        glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0, 1, 0));
        glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), Pitch, glm::vec3(1, 0, 0));
        glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), Roll, glm::vec3(0, 0, 1));

        return TempGLMToUHMat(Ry * Rx * Rz);
    }

    UHMatrix4x4 UHMatrixRotationAxis(UHVector3 Axis, float Angle)
    {
        glm::mat4x4 Result = glm::rotate(glm::mat4(1.0f), Angle, glm::vec3(Axis.X, Axis.Y, Axis.Z));
        return TempGLMToUHMat(Result);
    }

    // vector functions
    UHVector4 UHVector3Transform(UHVector3 V, UHMatrix4x4 M)
    {
        glm::vec4 Result = TempUHToGLMMat(M) * glm::vec4(V.X, V.Y, V.Z, 1.0f);
        return UHVector4(Result.x, Result.y, Result.z, Result.w);
    }

    UHVector3 UHVector3TransformNormal(UHVector3 V, UHMatrix4x4 M)
    {
        glm::vec3 Result = glm::mat3(TempUHToGLMMat(M)) * glm::vec3(V.X, V.Y, V.Z);
        return UHVector3(Result.x, Result.y, Result.z);
    }

    bool UHVector3InBound(UHVector3 Vec, UHVector3 Bounds)
    {
        // is the vector in extent? Assume the inputs are in local (relative) space already
        return Vec.X <= Bounds.X && Vec.X >= -Bounds.X
            && Vec.Y <= Bounds.Y && Vec.Y >= -Bounds.Y
            && Vec.Z <= Bounds.Z && Vec.Z >= -Bounds.Z;
    }

    UHVector3 UHVector3Round(UHVector3 V)
    {
        glm::vec3 FV(V.X,V.Y,V.Z);
        FV = glm::round(FV);

        return UHVector3(FV.x, FV.y, FV.z);
    }

    UHVector4 UHVector3Normalize(UHVector4 V)
    {
        // normalize 3d part of a vector4, the w component remains unchanged
        UHVector3 Result = UHVector3Normalize(UHVector3(V.X, V.Y, V.Z));
        return UHVector4(Result.X, Result.Y, Result.Z, V.W);
    }

    UHVector3 UHVector3Normalize(UHVector3 V)
    {
        glm::vec3 FV(V.X, V.Y, V.Z);
        FV = glm::normalize(FV);

        return UHVector3(FV.x, FV.y, FV.z);
    }

    UHVector4 UHQuaternionRotationMatrix(UHMatrix4x4 M)
    {
        glm::mat4x4 FM = TempUHToGLMMat(M);
        glm::quat Q = glm::quat_cast(FM);
        
        return UHVector4(Q.x, Q.y, Q.z, Q.w);
    }

    UHVector4 UHQuaternionMultiply(UHVector4 Q1, UHVector4 Q2)
    {
        glm::quat GQ1(Q1.W, Q1.X, Q1.Y, Q1.Z);
        glm::quat GQ2(Q2.W, Q2.X, Q2.Y, Q2.Z);
        GQ1 *= GQ2;

        return UHVector4(GQ1.x, GQ1.y, GQ1.z, GQ1.w);
    }

    float UHVector3Dot(UHVector4 V1, UHVector4 V2)
    {
        glm::vec3 GV1(V1.X, V1.Y, V1.Z);
        glm::vec3 GV2(V2.X, V2.Y, V2.Z);

        return glm::dot(GV1, GV2);
    }

    float UHVector3Dot(UHVector3 V1, UHVector3 V2)
    {
        glm::vec3 GV1(V1.X, V1.Y, V1.Z);
        glm::vec3 GV2(V2.X, V2.Y, V2.Z);

        return glm::dot(GV1, GV2);
    }

    UHVector4 UHVector4Transform(UHVector4 V, UHMatrix4x4 M)
    {
        glm::vec4 Result = TempUHToGLMMat(M) * glm::vec4(V.X, V.Y, V.Z, V.W);
        return UHVector4(Result.x, Result.y, Result.z, Result.w);
    }

    UHVector4 UHVector4Multiply(UHVector4 V1, UHVector4 V2)
    {
        glm::vec4 GV1(V1.X, V1.Y, V1.Z, V1.W);
        glm::vec4 GV2(V2.X, V2.Y, V2.Z, V2.W);
        GV1 *= GV2;

        return UHVector4(GV1.x, GV1.y, GV1.z, GV1.w);
    }

    UHVector4 UHVector4SplatZ(UHVector4 V)
    {
        glm::vec4 GV(V.X, V.Y, V.Z, V.W);
        GV = glm::splatZ(GV);

        return UHVector4(GV.x, GV.y, GV.z, GV.w);
    }

    UHVector4 UHVector4SplatW(UHVector4 V)
    {
        glm::vec4 GV(V.X, V.Y, V.Z, V.W);
        GV = glm::splatW(GV);

        return UHVector4(GV.x, GV.y, GV.z, GV.w);
    }

    UHVector4 UHVector4Reciprocal(UHVector4 V)
    {
        glm::vec4 GV(V.X, V.Y, V.Z, V.W);
        GV = 1.0f / GV;

        return UHVector4(GV.x, GV.y, GV.z, GV.w);
    }

    // Transform a vector using a rotation expressed as a unit quaternion
    UHVector3 UHVector3Rotate(UHVector4 V, UHVector4 Q)
    {
        glm::vec3 GV(V.X, V.Y, V.Z);
        glm::quat GQ(Q.W, Q.X, Q.Y, Q.Z);
        glm::vec3 Result = GQ * GV;

        return UHVector3(Result.x, Result.y, Result.z);
    }

    float UHVector4Dot(UHVector4 V1, UHVector4 V2)
    {
        glm::vec4 GV1(V1.X, V1.Y, V1.Z, V1.W);
        glm::vec4 GV2(V2.X, V2.Y, V2.Z, V2.W);

        return glm::dot(GV1, GV2);
    }

    UHVector3 UHVector3Abs(UHVector3 V)
    {
        glm::vec3 GV(V.X, V.Y, V.Z);
        GV = glm::abs(GV);

        return UHVector3(GV.x, GV.y, GV.z);
    }

    // plane functions
    UHVector4 UHPlaneTransform(UHVector4 Plane, UHVector4 Rotation, UHVector3 Translation)
    {
        UHVector3 VNormal = UHVector3Rotate(Plane, Rotation);
        float Dist = Plane.W - UHVector3Dot(VNormal, Translation);

        return UHVector4(VNormal.X, VNormal.Y, VNormal.Z, Dist);
    }

    UHVector4 UHPlaneNormalize(UHVector4 Plane)
    {
        float Length = glm::length(glm::vec3(Plane.X, Plane.Y, Plane.Z));

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
        float Radius = UHVector3Dot(Extents, UHVector3Abs(UHVector3(Plane.X, Plane.Y, Plane.Z)));

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

// ----------------------------------------------------------- UHVector3 operators
UHVector3 operator*(const UHVector3& InVector, float InFloat)
{
    glm::vec3 V(InVector.X, InVector.Y, InVector.Z);
    V *= InFloat;

    return UHVector3(V.x, V.y, V.z);
}

UHVector3 operator+(const UHVector3& InVector, const UHVector3& InVector2)
{
    glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
    glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
    V1 += V2;

    return UHVector3(V1.x, V1.y, V1.z);
}

UHVector3 operator-(const UHVector3& InVector, const UHVector3& InVector2)
{
    glm::vec3 V1(InVector.X, InVector.Y, InVector.Z);
    glm::vec3 V2(InVector2.X, InVector2.Y, InVector2.Z);
    V1 -= V2;

    return UHVector3(V1.x, V1.y, V1.z);
}

bool operator==(const UHVector3& InVector, const UHVector3& InVector2)
{
    return InVector.X == InVector2.X && InVector.Y == InVector2.Y && InVector.Z == InVector2.Z;
}

// ----------------------------------------------------------- UHVector4 operators
UHVector4 operator/=(const UHVector4& InVector, float InFloat)
{
    UHVector4 V = InVector;
    V.X /= InFloat;
    V.Y /= InFloat;
    V.Z /= InFloat;
    V.W /= InFloat;

    return V;
}

// ----------------------------------------------------------- UHMatrix4x4 operator
UHMatrix4x4 operator*(const UHMatrix4x4& InA, const UHMatrix4x4& InB)
{
    glm::mat4x4 M1 = TempUHToGLMMat(InA);
    glm::mat4x4 M2 = TempUHToGLMMat(InB);

    // the order matters!
    return TempGLMToUHMat(M2 * M1);
}

void UHMatrix4x4::CreateFromVector(UHMatrix4x4& OutM, const UHVector4& R0, const UHVector4& R1, const UHVector4& R2, const UHVector4& R3) noexcept
{
    OutM.M[0][0] = R0.X;
    OutM.M[0][1] = R0.Y;
    OutM.M[0][2] = R0.Z;
    OutM.M[0][3] = R0.W;

    OutM.M[1][0] = R1.X;
    OutM.M[1][1] = R1.Y;
    OutM.M[1][2] = R1.Z;
    OutM.M[1][3] = R1.W;

    OutM.M[2][0] = R2.X;
    OutM.M[2][1] = R2.Y;
    OutM.M[2][2] = R2.Z;
    OutM.M[2][3] = R2.W;

    OutM.M[3][0] = R3.X;
    OutM.M[3][1] = R3.Y;
    OutM.M[3][2] = R3.Z;
    OutM.M[3][3] = R3.W;
}

// ----------------------------------------------------------- UHBoundingBox
bool UHBoundingBox::Contains(UHVector3 Point) const noexcept
{
    glm::vec3 C(Center.X, Center.Y, Center.Z);
    glm::vec3 E(Extents.X, Extents.Y, Extents.Z);
    glm::vec3 V = glm::vec3(Point.X, Point.Y, Point.Z) - C;

    return UHMathHelpers::UHVector3InBound(UHVector3(V.x, V.y, V.z), Extents);
}

bool UHBoundingBox::ContainedBy(UHVector4 Plane0, UHVector4 Plane1, UHVector4 Plane2,
    UHVector4 Plane3, UHVector4 Plane4, UHVector4 Plane5) const noexcept
{
    UHVector4 CenterWithW(Center.X, Center.Y, Center.Z, 1.0f);

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
    glm::vec3 C(Center.X, Center.Y, Center.Z);
    glm::vec3 E(Extents.X, Extents.Y, Extents.Z);
    glm::vec3 Offset(GBoxOffset[0].X, GBoxOffset[0].Y, GBoxOffset[0].Z);
    glm::vec3 Corner = E * Offset + C;

    UHVector4 TransformedCorner = UHMathHelpers::UHVector3Transform(UHVector3(Corner.x, Corner.y, Corner.z), M);
    Corner = glm::vec3(TransformedCorner.X, TransformedCorner.Y, TransformedCorner.Z);

    glm::vec3 Min, Max;
    Min = Max = Corner;

    for (size_t I = 1; I < CORNER_COUNT; I++)
    {
        Offset = glm::vec3(GBoxOffset[I].X, GBoxOffset[I].Y, GBoxOffset[I].Z);
        Corner = E * Offset + C;

        TransformedCorner = UHMathHelpers::UHVector3Transform(UHVector3(Corner.x, Corner.y, Corner.z), M);
        Corner = glm::vec3(TransformedCorner.X, TransformedCorner.Y, TransformedCorner.Z);

        Min = glm::min(Min, Corner);
        Max = glm::max(Max, Corner);
    }

    C = (Min + Max) * 0.5f;
    E = (Max - Min) * 0.5f;
    Out.Center = UHVector3(C.x, C.y, C.z);
    Out.Extents = UHVector3(E.x, E.y, E.z);
}

void UHBoundingBox::GetCorners(UHVector3* Corners) const noexcept
{
    assert(Corners != nullptr);

    // Load the box
    glm::vec3 C(Center.X, Center.Y, Center.Z);
    glm::vec3 E(Extents.X, Extents.Y, Extents.Z);

    for (size_t I = 0; I < CORNER_COUNT; I++)
    {
        glm::vec3 Offset(GBoxOffset[I].X, GBoxOffset[I].Y, GBoxOffset[I].Z);
        glm::vec3 TempCorner = E * Offset + C;

        Corners[I] = UHVector3(TempCorner.x, TempCorner.y, TempCorner.z);
    }
}

void UHBoundingBox::CreateMerged(UHBoundingBox& Out, const UHBoundingBox& b1, const UHBoundingBox& b2) noexcept
{
    glm::vec3 C1(b1.Center.X, b1.Center.Y, b1.Center.Z);
    glm::vec3 E1(b1.Extents.X, b1.Extents.Y, b1.Extents.Z);

    glm::vec3 C2(b2.Center.X, b2.Center.Y, b2.Center.Z);
    glm::vec3 E2(b2.Extents.X, b2.Extents.Y, b2.Extents.Z);

    glm::vec3 Min = glm::min(C1 - E1, C2 - E2);
    glm::vec3 Max = glm::max(C1 + E1, C2 + E2);

    glm::vec3 NewC = (Min + Max) * 0.5f;
    glm::vec3 NewE = (Max - Min) * 0.5f;

    Out.Center = UHVector3(NewC.x, NewC.y, NewC.z);
    Out.Extents = UHVector3(NewE.x, NewE.y, NewE.z);
}

// ----------------------------------------------------------- UHBoundingFrustum
void UHBoundingFrustum::Transform(UHBoundingFrustum& Out, UHMatrix4x4 M) const noexcept
{
    // transform the center
    UHVector4 TransformedOrigin = UHMathHelpers::UHVector3Transform(Origin, M);

    // Composite the frustum rotation and the transform rotation
    UHVector4 R0(M.M[0][0], M.M[0][1], M.M[0][2], M.M[0][3]);
    UHVector4 R1(M.M[1][0], M.M[1][1], M.M[1][2], M.M[1][3]);
    UHVector4 R2(M.M[2][0], M.M[2][1], M.M[2][2], M.M[2][3]);
    UHVector4 R3(0, 0, 0, 1);
    R0 = UHMathHelpers::UHVector3Normalize(R0);
    R1 = UHMathHelpers::UHVector3Normalize(R1);
    R2 = UHMathHelpers::UHVector3Normalize(R2);

    UHMatrix4x4 NormalizedM;
    UHMatrix4x4::CreateFromVector(NormalizedM, R0, R1, R2, R3);
    UHVector4 Rotation = UHMathHelpers::UHQuaternionRotationMatrix(NormalizedM);

    // output transformed origin and orientation
    Out.Origin = UHVector3(TransformedOrigin.X, TransformedOrigin.Y, TransformedOrigin.Z);
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

    Out.RightSlope = Points[0].X;
    Out.LeftSlope = Points[1].X;
    Out.TopSlope = Points[2].Y;
    Out.BottomSlope = Points[3].Y;

    // Compute near and far.
    Points[4] = UHMathHelpers::UHVector4Multiply(Points[4], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatW(Points[4])));
    Points[5] = UHMathHelpers::UHVector4Multiply(Points[5], UHMathHelpers::UHVector4Reciprocal(UHMathHelpers::UHVector4SplatW(Points[5])));

    if (rhcoords)
    {
        Out.Near = Points[5].Z;
        Out.Far = Points[4].Z;
    }
    else
    {
        Out.Near = Points[4].Z;
        Out.Far = Points[5].Z;
    }
}