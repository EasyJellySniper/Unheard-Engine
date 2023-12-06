#pragma once

// UHE uses DX math library, it's not only a mature library but also well-optimized (with SSE instructions for example)
#include <cmath>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <limits>
#include <algorithm>
#include <vector>
#include <memory>
using namespace DirectX;

static float GEpsilon = std::numeric_limits<float>::epsilon();
static float GWorldMax = static_cast<float>(1 << 20);

template <typename T>
using UniquePtr = std::unique_ptr<T>;

#define MakeUnique std::make_unique
#define StdBind std::bind
#define UHSTRINGA "std::string"
#define UHSTRINGW "std::wstring"
#define UHINDEXNONE -1

namespace MathHelpers
{
    bool IsValidVector(XMFLOAT3 InVector);

    bool IsVectorNearlyZero(XMFLOAT3 InVector);

    bool IsVectorNearlyZero(XMFLOAT4 InVector);

    bool IsVectorEqual(XMFLOAT2 InA, XMFLOAT2 InB);

    bool IsVectorEqual(XMFLOAT3 InA, XMFLOAT3 InB);

    bool IsVectorEqual(XMFLOAT4 InA, XMFLOAT4 InB);

    DirectX::XMFLOAT4X4 Identity4x4();

    DirectX::XMFLOAT3X4 MatrixTo3x4(XMFLOAT4X4 InMatrix);

    DirectX::XMFLOAT3X4 Identity3x4();

    // Returns random float in [0, 1).
    float RandFloat();

    // matrix to pitch yaw roll, use for display only!
    void MatrixToPitchYawRoll(XMFLOAT4X4 InMat, float& Pitch, float& Yaw, float& Roll);

    XMFLOAT3 MinVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

    XMFLOAT3 MaxVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

    float Halton(int32_t Index, int32_t Base);

    float Lerp(const float& InVal1, const float& InVal2, const float& T);
    XMFLOAT3 LerpVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2, const float& T);

    float VectorDistanceSqr(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

    template <typename T>
    T CubicInterpolate(const std::vector<T>& InData, const float InWeight);

    template <typename T>
    T BicubicInterpolate(const std::vector<T>& InData, const float InWeight1, const float InWeight2);

    float RoundUpToClosestPowerOfTwo(float InVal);
    int32_t RoundUpDivide(int32_t InVal, int32_t Divisor);
}

// operator for XMFLOAT3 multipication
XMFLOAT3 operator*(const XMFLOAT3& InVector, float InFloat);

// operator for XMFLOAT3 addition
XMFLOAT3 operator+(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

// operator for XMFLOAT3 subtraction
XMFLOAT3 operator-(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

bool operator==(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

struct UHColorRGB
{
    UHColorRGB();
    UHColorRGB(const float InR, const float InG, const float InB);

    static float SquareDiff(const UHColorRGB& ColorA, const UHColorRGB& ColorB);
	static UHColorRGB Lerp(const UHColorRGB& ColorA, const UHColorRGB& ColorB, const float& FactorA, const float& FactorB);
    static UHColorRGB Min(const UHColorRGB& ColorA, const UHColorRGB& ColorB);
    static UHColorRGB Max(const UHColorRGB& ColorA, const UHColorRGB& ColorB);
    void Clamp();
    void Abs();
    float Lumin() const;
    float Length() const;

	UHColorRGB& operator+=(const UHColorRGB& Rhs);
	UHColorRGB& operator/=(const float& Rhs);
    friend UHColorRGB operator+(const UHColorRGB& Lhs, const UHColorRGB& Rhs);
    friend UHColorRGB operator-(const UHColorRGB& Lhs, const UHColorRGB& Rhs);

	float R;
	float G;
	float B;
};

UHColorRGB operator*(float Lhs, const UHColorRGB& Rhs);
UHColorRGB operator/(const UHColorRGB& Lhs, float Rhs);

struct UHColorRGBInt
{
    UHColorRGBInt();

    uint32_t R;
    uint32_t G;
    uint32_t B;
};