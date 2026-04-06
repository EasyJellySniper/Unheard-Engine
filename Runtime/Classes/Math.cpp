#include "Math.h"
#include "Runtime/CoreGlobals.h"
#include <cmath>

// @TODO: Replace DX math library by GLM
#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

namespace UHMathHelpers
{
    bool IsValidVector(UHVector3 InVector)
    {
        XMVECTOR Result = XMVectorIsNaN(XMLoadFloat3((XMFLOAT3*)&InVector));
        return Result.m128_u32[0] == 0 && Result.m128_u32[1] == 0 && Result.m128_u32[2] == 0;
    }

    bool IsVectorNearlyZero(UHVector3 InVector)
    {
        XMVECTOR Result = XMVectorNearEqual(XMLoadFloat3((XMFLOAT3*)&InVector), XMVectorZero(), XMVectorSet(GEpsilon, GEpsilon, GEpsilon, GEpsilon));
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0;
    }

    bool IsVectorNearlyZero(UHVector4 InVector)
    {
        XMVECTOR Result = XMVectorNearEqual(XMLoadFloat4((XMFLOAT4*)&InVector), XMVectorZero(), XMVectorSet(GEpsilon, GEpsilon, GEpsilon, GEpsilon));
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0 && Result.m128_u32[3] > 0;
    }

    bool IsVectorEqual(UHVector2 InA, UHVector2 InB)
    {
        XMVECTOR A = XMLoadFloat2((XMFLOAT2*)&InA);
        XMVECTOR B = XMLoadFloat2((XMFLOAT2*)&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0;
    }

    bool IsVectorEqual(UHVector3 InA, UHVector3 InB)
    {
        XMVECTOR A = XMLoadFloat3((XMFLOAT3*)&InA);
        XMVECTOR B = XMLoadFloat3((XMFLOAT3*)&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0;
    }

    bool IsVectorEqual(UHVector4 InA, UHVector4 InB)
    {
        XMVECTOR A = XMLoadFloat4((XMFLOAT4*)&InA);
        XMVECTOR B = XMLoadFloat4((XMFLOAT4*)&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0 && Result.m128_u32[3] > 0;
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
        XMMATRIX M = XMLoadFloat4x4((XMFLOAT4X4*)&InMat);
        M = XMMatrixTranspose(M);

        XMFLOAT4X4 OutMat;
        XMStoreFloat4x4(&OutMat, M);

        if (OutMat(0, 0) == 1.0f)
        {
            Yaw = std::atan2f(OutMat(0, 2), OutMat(2, 3));
            Pitch = 0;
            Roll = 0;

        }
        else if (OutMat(0, 0) == -1.0f)
        {
            Yaw = std::atan2f(OutMat(0, 2), OutMat(2, 3));
            Pitch = 0;
            Roll = 0;
        }
        else
        {
            Yaw = std::atan2(-OutMat(2, 0), OutMat(0, 0));
            Roll = std::asin(OutMat(1, 0));
            Pitch = std::atan2(-OutMat(1, 2), OutMat(1, 1));
        }

        // to degrees
        Pitch = XMConvertToDegrees(Pitch);
        Yaw = XMConvertToDegrees(Yaw);
        Roll = XMConvertToDegrees(Roll);
    }

    UHVector3 MinVector(const UHVector3& InVector, const UHVector3& InVector2)
    {
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, XMVectorMin(XMLoadFloat3((XMFLOAT3*)&InVector), XMLoadFloat3((XMFLOAT3*)&InVector2)));

        return UHVector3(Result.x, Result.y, Result.z);
    }

    UHVector3 MaxVector(const UHVector3& InVector, const UHVector3& InVector2)
    {
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, XMVectorMax(XMLoadFloat3((XMFLOAT3*)&InVector), XMLoadFloat3((XMFLOAT3*)&InVector2)));

        return UHVector3(Result.x, Result.y, Result.z);
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
        XMVECTOR Lerped = XMVectorLerp(XMLoadFloat3((XMFLOAT3*)&InVector), XMLoadFloat3((XMFLOAT3*)&InVector2), T);
        XMFLOAT3 NewPos;
        XMStoreFloat3(&NewPos, Lerped);

        return UHVector3(NewPos.x, NewPos.y, NewPos.z);
    }

    float VectorDistanceSqr(const UHVector3& InVector, const UHVector3& InVector2)
    {
        // return squared vector distance
        XMVECTOR A = XMLoadFloat3((XMFLOAT3*)&InVector);
        XMVECTOR B = XMLoadFloat3((XMFLOAT3*)&InVector2);
        return XMVector3LengthSq(A - B).m128_f32[0];
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
        XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&M);
        FM = XMMatrixTranspose(FM);

        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, FM);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }
    
    UHMatrix4x4 UHMatrixTranslation(UHVector3 V)
    {
        XMMATRIX M = XMMatrixTranslation(V.X, V.Y, V.Z);

        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, M);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixScaling(UHVector3 V)
    {
        XMMATRIX M = XMMatrixScaling(V.X, V.Y, V.Z);

        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, M);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixPerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
    {
        const XMMATRIX M = XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ);
        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, M);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixPerspectiveFovRH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
    {
        const XMMATRIX M = XMMatrixPerspectiveFovRH(FovAngleY, AspectRatio, NearZ, FarZ);
        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, M);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixLookToRH(UHVector3 Position, UHVector3 Forward, UHVector3 Up)
    {
        const XMVECTOR U = XMLoadFloat3((XMFLOAT3*)&Up);
        const XMVECTOR F = XMLoadFloat3((XMFLOAT3*)&Forward);
        const XMVECTOR P = XMLoadFloat3((XMFLOAT3*)&Position);

        const XMMATRIX View = XMMatrixLookToRH(P, F, U);
        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, View);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHVector4 UHMatrixDeterminant(UHMatrix4x4 M)
    {
        XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&M);
        XMVECTOR V = XMMatrixDeterminant(FM);

        XMFLOAT4 Result;
        XMStoreFloat4(&Result, V);
        return UHVector4(Result.x, Result.y, Result.z, Result.w);
    }

    UHMatrix4x4 UHMatrixInverse(UHVector4 Det, UHMatrix4x4 M)
    {
        XMVECTOR FV = XMLoadFloat4((XMFLOAT4*)&Det);
        XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&M);
        XMMATRIX InvM = XMMatrixInverse(&FV, FM);
        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, InvM);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixRotationRollPitchYaw(float Pitch, float Yaw, float Roll) noexcept
    {
        XMMATRIX FM = XMMatrixRotationRollPitchYaw(Pitch, Yaw, Roll);
        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, FM);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    UHMatrix4x4 UHMatrixRotationAxis(UHVector3 Axis, float Angle)
    {
        XMVECTOR V = XMLoadFloat3((XMFLOAT3*)&Axis);
        XMMATRIX M = XMMatrixRotationAxis(V, Angle);

        XMFLOAT4X4 OutM;
        XMStoreFloat4x4(&OutM, M);

        UHMatrix4x4 ResultM;
        for (int32_t I = 0; I < 4; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                ResultM.M[I][J] = OutM.m[I][J];
            }
        }

        return ResultM;
    }

    // vector functions
    UHVector4 UHVector3Transform(UHVector3 V, UHMatrix4x4 M)
    {
        XMVECTOR P = XMLoadFloat3((XMFLOAT3*)&V);
        XMMATRIX VP = XMLoadFloat4x4((XMFLOAT4X4*)&M);
        P = XMVector3Transform(P, VP);

        XMFLOAT4 Result;
        XMStoreFloat4(&Result, P);
        return UHVector4(Result.x, Result.y, Result.z, Result.w);
    }

    UHVector3 UHVector3TransformNormal(UHVector3 V, UHMatrix4x4 M)
    {
        XMVECTOR FV = XMLoadFloat3((XMFLOAT3*)&V);
        XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&M);

        FV = XMVector3TransformNormal(FV, FM);
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, FV);

        return UHVector3(Result.x, Result.y, Result.z);
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
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3((XMFLOAT3*)&InVector) * InFloat);

    return UHVector3(OutVector.x, OutVector.y, OutVector.z);
}

UHVector3 operator+(const UHVector3& InVector, const UHVector3& InVector2)
{
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3((XMFLOAT3*)&InVector) + XMLoadFloat3((XMFLOAT3*)&InVector2));

    return UHVector3(OutVector.x, OutVector.y, OutVector.z);
}

UHVector3 operator-(const UHVector3& InVector, const UHVector3& InVector2)
{
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3((XMFLOAT3*)&InVector) - XMLoadFloat3((XMFLOAT3*)&InVector2));

    return UHVector3(OutVector.x, OutVector.y, OutVector.z);
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
    XMMATRIX M1 = XMLoadFloat4x4((XMFLOAT4X4*)&InA);
    XMMATRIX M2 = XMLoadFloat4x4((XMFLOAT4X4*)&InB);
    XMFLOAT4X4 OutM;
    XMStoreFloat4x4(&OutM, XMMatrixMultiply(M1, M2));

    UHMatrix4x4 ResultM;
    for (int32_t I = 0; I < 4; I++)
    {
        for (int32_t J = 0; J < 4; J++)
        {
            ResultM.M[I][J] = OutM.m[I][J];
        }
    }

    return ResultM;
}

// ----------------------------------------------------------- UHBoundingBox
bool UHBoundingBox::Contains(UHVector3 Point) const noexcept
{
    XMVECTOR vCenter = XMLoadFloat3((XMFLOAT3*)&Center);
    XMVECTOR vExtents = XMLoadFloat3((XMFLOAT3*)&Extents);

    return XMVector3InBounds(XMVectorSubtract(XMLoadFloat3((XMFLOAT3*)&Point), vCenter), vExtents);
}

void UHBoundingBox::Transform(UHBoundingBox& Out, UHMatrix4x4 M) const noexcept
{
    // Load center and extents.
    XMVECTOR vCenter = XMLoadFloat3((XMFLOAT3*)&Center);
    XMVECTOR vExtents = XMLoadFloat3((XMFLOAT3*)&Extents);

    // Compute and transform the corners and find new min/max bounds.
    XMVECTOR Corner = XMVectorMultiplyAdd(vExtents, g_BoxOffset[0], vCenter);
    XMMATRIX XM = XMLoadFloat4x4((XMFLOAT4X4*)&M);
    Corner = XMVector3Transform(Corner, XM);

    XMVECTOR Min, Max;
    Min = Max = Corner;

    for (size_t i = 1; i < CORNER_COUNT; ++i)
    {
        Corner = XMVectorMultiplyAdd(vExtents, g_BoxOffset[i], vCenter);
        Corner = XMVector3Transform(Corner, XM);

        Min = XMVectorMin(Min, Corner);
        Max = XMVectorMax(Max, Corner);
    }

    // Store center and extents.
    XMFLOAT3 Center, Extents;
    XMStoreFloat3(&Center, XMVectorScale(XMVectorAdd(Min, Max), 0.5f));
    XMStoreFloat3(&Extents, XMVectorScale(XMVectorSubtract(Max, Min), 0.5f));
    
    Out.Center = UHVector3(Center.x, Center.y, Center.z);
    Out.Extents = UHVector3(Extents.x, Extents.y, Extents.z);
}

void UHBoundingBox::GetCorners(UHVector3* Corners) const noexcept
{
    assert(Corners != nullptr);

    // Load the box
    XMVECTOR vCenter = XMLoadFloat3((XMFLOAT3*)&Center);
    XMVECTOR vExtents = XMLoadFloat3((XMFLOAT3*)&Extents);

    for (size_t i = 0; i < CORNER_COUNT; ++i)
    {
        XMVECTOR C = XMVectorMultiplyAdd(vExtents, g_BoxOffset[i], vCenter);
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, C);
        Corners[i] = UHVector3(Result.x, Result.y, Result.z);
    }
}

void UHBoundingBox::CreateMerged(UHBoundingBox& Out, const UHBoundingBox& b1, const UHBoundingBox& b2) noexcept
{
    XMVECTOR b1Center = XMLoadFloat3((XMFLOAT3*)&b1.Center);
    XMVECTOR b1Extents = XMLoadFloat3((XMFLOAT3*)&b1.Extents);

    XMVECTOR b2Center = XMLoadFloat3((XMFLOAT3*)&b2.Center);
    XMVECTOR b2Extents = XMLoadFloat3((XMFLOAT3*)&b2.Extents);

    XMVECTOR Min = XMVectorSubtract(b1Center, b1Extents);
    Min = XMVectorMin(Min, XMVectorSubtract(b2Center, b2Extents));

    XMVECTOR Max = XMVectorAdd(b1Center, b1Extents);
    Max = XMVectorMax(Max, XMVectorAdd(b2Center, b2Extents));

    assert(XMVector3LessOrEqual(Min, Max));

    XMFLOAT3 Center, Extents;
    XMStoreFloat3(&Center, XMVectorScale(XMVectorAdd(Min, Max), 0.5f));
    XMStoreFloat3(&Extents, XMVectorScale(XMVectorSubtract(Max, Min), 0.5f));

    Out.Center = UHVector3(Center.x, Center.y, Center.z);
    Out.Extents = UHVector3(Extents.x, Extents.y, Extents.z);
}

// ----------------------------------------------------------- UHBoundingFrustum
void UHBoundingFrustum::Transform(UHBoundingFrustum& Out, UHMatrix4x4 M) const noexcept
{
    BoundingFrustum DXFrustum{};
    DXFrustum.Origin = XMFLOAT3(Origin.X, Origin.Y, Origin.Z);
    DXFrustum.Orientation = XMFLOAT4(Orientation.X, Orientation.Y, Orientation.Z, Orientation.W);
    DXFrustum.RightSlope = RightSlope;
    DXFrustum.LeftSlope = LeftSlope;
    DXFrustum.TopSlope = TopSlope;
    DXFrustum.BottomSlope = BottomSlope;
    DXFrustum.Near = Near;
    DXFrustum.Far = Far;

    XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&M);
    DXFrustum.Transform(DXFrustum, FM);

    Out.Origin = UHVector3(DXFrustum.Origin.x, DXFrustum.Origin.y, DXFrustum.Origin.z);
    Out.Orientation = UHVector4(DXFrustum.Orientation.x, DXFrustum.Orientation.y, DXFrustum.Orientation.z, DXFrustum.Orientation.w);
    Out.RightSlope = DXFrustum.RightSlope;
    Out.LeftSlope = DXFrustum.LeftSlope;
    Out.TopSlope = DXFrustum.TopSlope;
    Out.BottomSlope = DXFrustum.BottomSlope;
    Out.Near = DXFrustum.Near;
    Out.Far = DXFrustum.Far;
}

bool UHBoundingFrustum::Contains(const UHBoundingBox& Box) const noexcept
{
    BoundingFrustum DXFrustum{};
    DXFrustum.Origin = XMFLOAT3(Origin.X, Origin.Y, Origin.Z);
    DXFrustum.Orientation = XMFLOAT4(Orientation.X, Orientation.Y, Orientation.Z, Orientation.W);
    DXFrustum.RightSlope = RightSlope;
    DXFrustum.LeftSlope = LeftSlope;
    DXFrustum.TopSlope = TopSlope;
    DXFrustum.BottomSlope = BottomSlope;
    DXFrustum.Near = Near;
    DXFrustum.Far = Far;

    BoundingBox DXBox{};
    DXBox.Center = XMFLOAT3(Box.Center.X, Box.Center.Y, Box.Center.Z);
    DXBox.Extents = XMFLOAT3(Box.Extents.X, Box.Extents.Y, Box.Extents.Z);

    return DXFrustum.Contains(DXBox);
}

void UHBoundingFrustum::CreateFromMatrix(UHBoundingFrustum& Out, UHMatrix4x4 Projection, bool rhcoords) noexcept
{
    BoundingFrustum DXFrustum{};
    XMMATRIX FM = XMLoadFloat4x4((XMFLOAT4X4*)&Projection);
    BoundingFrustum::CreateFromMatrix(DXFrustum, FM, rhcoords);

    Out.Origin = UHVector3(DXFrustum.Origin.x, DXFrustum.Origin.y, DXFrustum.Origin.z);
    Out.Orientation = UHVector4(DXFrustum.Orientation.x, DXFrustum.Orientation.y, DXFrustum.Orientation.z, DXFrustum.Orientation.w);
    Out.RightSlope = DXFrustum.RightSlope;
    Out.LeftSlope = DXFrustum.LeftSlope;
    Out.TopSlope = DXFrustum.TopSlope;
    Out.BottomSlope = DXFrustum.BottomSlope;
    Out.Near = DXFrustum.Near;
    Out.Far = DXFrustum.Far;
}