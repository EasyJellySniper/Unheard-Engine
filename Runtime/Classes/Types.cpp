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

    bool IsVectorEqual(XMFLOAT2 InA, XMFLOAT2 InB)
    {
        XMVECTOR A = XMLoadFloat2(&InA);
        XMVECTOR B = XMLoadFloat2(&InB);
        XMVECTOR Result = XMVectorEqual(A, B);
        return Result.m128_u32[0] > 0 && Result.m128_u32[1] > 0;
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
    // https://en.wikipedia.org/wiki/Euler_angles#Conversion_to_other_orientation_representations
    // the Y1Z2X3 case
    void MatrixToPitchYawRoll(XMFLOAT4X4 InMat, float& Pitch, float& Yaw, float& Roll)
    {
        XMMATRIX M = XMLoadFloat4x4(&InMat);
        M = XMMatrixTranspose(M);
        XMStoreFloat4x4(&InMat, M);

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
            Roll = std::asin(InMat(1, 0));
            Pitch = std::atan2(-InMat(1, 2), InMat(1, 1));
        }

        // to degrees
        Pitch = XMConvertToDegrees(Pitch);
        Yaw = XMConvertToDegrees(Yaw);
        Roll = XMConvertToDegrees(Roll);
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

    float Lerp(const float& InVal1, const float& InVal2, const float& T)
    {
        return InVal1 * (1.0f - T) + InVal2 * T;
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

    // bicubic interpolation, weight ranged from 0 to 1
    // reference: https://www.paulinternet.nl/?page=bicubic
    template <typename T>
    T CubicInterpolate(const std::vector<T>& InData, const float InWeight)
    {
        assert(InData.size() == 4);
        return InData[1]
            + 0.5f * InWeight * (InData[2] - InData[0])
            + InWeight * InWeight * (InData[0] - 2.5f * InData[1] + 2.0f * InData[2] - 0.5f * InData[3])
            + InWeight * InWeight * InWeight * (-0.5f * InData[0] + 1.5f * InData[1] - 1.5f * InData[2] + 0.5f * InData[3]);
    }

    template <typename T>
    T BicubicInterpolate(const std::vector<T>& InData, const float InWeight1, const float InWeight2)
    {
        // must be 16 (4x4) at least
        assert(InData.size() == 16);
        float Weight1 = std::clamp(InWeight1, 0.0f, 1.0f);
        float Weight2 = std::clamp(InWeight2, 0.0f, 1.0f);

        std::vector<T> FirstResult(4);
        for (size_t Idx = 0; Idx < 4; Idx++)
        {
            FirstResult[Idx] = CubicInterpolate(std::vector<T>(InData.begin() + Idx * 4, InData.begin() + 4 * (Idx + 1)), InWeight2);
        }

        return CubicInterpolate(FirstResult, InWeight1);
    }

    template float BicubicInterpolate(const std::vector<float>& InData, const float InWeight1, const float InWeight2);
    template UHColorRGB BicubicInterpolate(const std::vector<UHColorRGB>& InData, const float InWeight1, const float InWeight2);

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

bool operator==(const XMFLOAT3& InVector, const XMFLOAT3& InVector2)
{
    return InVector.x == InVector2.x && InVector.y == InVector2.y && InVector.z == InVector2.z;
}

// UHColorRGB
UHColorRGB::UHColorRGB()
    : R(0)
    , G(0)
    , B(0)
{

}

UHColorRGB::UHColorRGB(const float InR, const float InG, const float InB)
    : R(InR)
    , G(InG)
    , B(InB)
{

}

float UHColorRGB::SquareDiff(const UHColorRGB& ColorA, const UHColorRGB& ColorB)
{
    float Result = (ColorA.R - ColorB.R) * (ColorA.R - ColorB.R)
        + (ColorA.G - ColorB.G) * (ColorA.G - ColorB.G)
        + (ColorA.B - ColorB.B) * (ColorA.B - ColorB.B);

    return Result;
}

UHColorRGB UHColorRGB::Lerp(const UHColorRGB& ColorA, const UHColorRGB& ColorB, const float& FactorA, const float& FactorB)
{
    UHColorRGB Result;
    float Divider = FactorA + FactorB;
    Result.R = (FactorA * ColorA.R + FactorB * ColorB.R) / Divider;
    Result.G = (FactorA * ColorA.G + FactorB * ColorB.G) / Divider;
    Result.B = (FactorA * ColorA.B + FactorB * ColorB.B) / Divider;

    return Result;
}

UHColorRGB UHColorRGB::Min(const UHColorRGB& ColorA, const UHColorRGB& ColorB)
{
    UHColorRGB Result;
    Result.R = std::min(ColorA.R, ColorB.R);
    Result.G = std::min(ColorA.G, ColorB.G);
    Result.B = std::min(ColorA.B, ColorB.B);
    return Result;
}

UHColorRGB UHColorRGB::Max(const UHColorRGB& ColorA, const UHColorRGB& ColorB)
{
    UHColorRGB Result;
    Result.R = std::max(ColorA.R, ColorB.R);
    Result.G = std::max(ColorA.G, ColorB.G);
    Result.B = std::max(ColorA.B, ColorB.B);
    return Result;
}

void UHColorRGB::Clamp()
{
    R = std::clamp(R, 0.0f, 255.0f);
    G = std::clamp(G, 0.0f, 255.0f);
    B = std::clamp(B, 0.0f, 255.0f);
}

void UHColorRGB::Abs()
{
    R = std::abs(R);
    G = std::abs(G);
    B = std::abs(B);
}

float UHColorRGB::Lumin() const
{
    return (0.2126f * R + 0.7152f * G + 0.0722f * B);
}

float UHColorRGB::Length() const
{
    return sqrtf((R * R + G * G + B * B));
}

UHColorRGB& UHColorRGB::operator+=(const UHColorRGB& Rhs)
{
    R += Rhs.R;
    G += Rhs.G;
    B += Rhs.B;
    return *this;
}

UHColorRGB& UHColorRGB::operator/=(const float& Rhs)
{
    R /= Rhs;
    G /= Rhs;
    B /= Rhs;
    return *this;
}

UHColorRGB operator+(const UHColorRGB& Lhs, const UHColorRGB& Rhs)
{
    UHColorRGB Result = Lhs;
    Result += Rhs;
    return Result;
}

UHColorRGB operator-(const UHColorRGB& Lhs, const UHColorRGB& Rhs)
{
    UHColorRGB Result;
    Result.R = Lhs.R - Rhs.R;
    Result.G = Lhs.G - Rhs.G;
    Result.B = Lhs.B - Rhs.B;
    return Result;
}

UHColorRGB operator*(float Lhs, const UHColorRGB& Rhs)
{
    UHColorRGB Result;
    Result.R = Lhs * Rhs.R;
    Result.G = Lhs * Rhs.G;
    Result.B = Lhs * Rhs.B;
    return Result;
}

UHColorRGB operator/(const UHColorRGB& Lhs, float Rhs)
{
    UHColorRGB Result;
    Result.R = Lhs.R / Rhs;
    Result.G = Lhs.G / Rhs;
    Result.B = Lhs.B / Rhs;
    return Result;
}

UHColorRGBInt::UHColorRGBInt()
    : R(0)
    , G(0)
    , B(0)
{

}