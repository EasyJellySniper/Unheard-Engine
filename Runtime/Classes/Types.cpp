#include "Types.h"

// https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-getting-started#type-usage-guidelines
// according to type usage guidelines from Microsoft, it's suggested to do computation with XMVECTOR/XMMATRIX
// but use XMFLOAT, XMFLOAT4X4 as class member

namespace MathHelpers
{
    bool IsValidVector(XMFLOAT3 InVector)
    {
        XMVECTOR Result = XMVectorIsNaN(XMLoadFloat3(&InVector));
        return Result.m128_u32[0] == 0 && Result.m128_u32[1] == 0 && Result.m128_u32[2] == 0;
    }

    bool IsVectorNearlyZero(XMFLOAT3 InVector)
    {
        XMVECTOR Result = XMVectorNearEqual(XMLoadFloat3(&InVector), XMVectorZero(), XMVectorSet(GEpsilon, GEpsilon, GEpsilon, GEpsilon));
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0;
    }

    bool IsVectorNearlyZero(XMFLOAT4 InVector)
    {
        XMVECTOR Result = XMVectorNearEqual(XMLoadFloat4(&InVector), XMVectorZero(), XMVectorSet(GEpsilon, GEpsilon, GEpsilon, GEpsilon));
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0 && Result.m128_u32[3] > 0;
    }

    bool IsVectorEqual(XMFLOAT3 InA, XMFLOAT3 InB)
    {
        XMVECTOR A = XMLoadFloat3(&InA);
        XMVECTOR B = XMLoadFloat3(&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0;
    }

    bool IsVectorEqual(XMFLOAT4 InA, XMFLOAT4 InB)
    {
        XMVECTOR A = XMLoadFloat4(&InA);
        XMVECTOR B = XMLoadFloat4(&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0 && Result.m128_u32[2] > 0 && Result.m128_u32[3] > 0;
    }

    DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    }

    DirectX::XMFLOAT3X4 MatrixTo3x4(XMFLOAT4X4 InMatrix)
    {
        XMFLOAT3X4 OutMatrix = XMFLOAT3X4();

        // just ignore the 4th row in 4X4 matrix
        for (int32_t I = 0; I < 3; I++)
        {
            for (int32_t J = 0; J < 4; J++)
            {
                OutMatrix.m[I][J] = InMatrix.m[I][J];
            }
        }

        return OutMatrix;
    }

    DirectX::XMFLOAT3X4 Identity3x4()
    {
        return MatrixTo3x4(Identity4x4());
    }

    // Returns random float in [0, 1).
    float RandFloat()
    {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }

    // matrix to pitch yaw roll, use for display only!
    void MatrixToPitchYawRoll(XMFLOAT4X4 InMat, float& Pitch, float& Yaw, float& Roll)
    {
        if (InMat(0, 0) == 1.0f)
        {
            Yaw = std::atan2f(InMat(0, 2), InMat(2, 3));
            Pitch = 0;
            Roll = 0;

        }
        else if (InMat(0, 0) == -1.0f)
        {
            Yaw = std::atan2f(InMat(0, 2), InMat(2, 3));
            Pitch = 0;
            Roll = 0;
        }
        else
        {
            Yaw = std::atan2(-InMat(2, 0), InMat(0, 0));
            Pitch = std::asin(InMat(1, 0));
            Roll = std::atan2(-InMat(1, 2), InMat(1, 1));
        }
    }

    XMFLOAT3 MinVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
    {
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, XMVectorMin(XMLoadFloat3(&InVector), XMLoadFloat3(&InVector2)));

        return Result;
    }

    XMFLOAT3 MaxVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
    {
        XMFLOAT3 Result;
        XMStoreFloat3(&Result, XMVectorMax(XMLoadFloat3(&InVector), XMLoadFloat3(&InVector2)));

        return Result;
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

    XMFLOAT3 LerpVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2, const float& T)
    {
        XMVECTOR Lerped = XMVectorLerp(XMLoadFloat3(&InVector), XMLoadFloat3(&InVector2), T);
        XMFLOAT3 NewPos;
        XMStoreFloat3(&NewPos, Lerped);
        return NewPos;
    }

    float VectorDistanceSqr(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
    {
        // return squared vector distance
        XMVECTOR A = XMLoadFloat3(&InVector);
        XMVECTOR B = XMLoadFloat3(&InVector2);
        return XMVector3LengthSq(A - B).m128_f32[0];
    }
}

// operator for XMFLOAT3 multipication
XMFLOAT3 operator*(const XMFLOAT3& InVector, float InFloat)
{
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3(&InVector) * InFloat);

    return OutVector;
}

// operator for XMFLOAT3 addition
XMFLOAT3 operator+(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
{
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3(&InVector) + XMLoadFloat3(&InVector2));

    return OutVector;
}

// operator for XMFLOAT3 subtraction
XMFLOAT3 operator-(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
{
    XMFLOAT3 OutVector;
    XMStoreFloat3(&OutVector, XMLoadFloat3(&InVector) - XMLoadFloat3(&InVector2));

    return OutVector;
}