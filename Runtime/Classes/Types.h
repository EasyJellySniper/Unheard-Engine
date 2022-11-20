#pragma once

// UH uses DX math library
#include <cmath>
#include <DirectXMath.h>
#include <limits>
#include <algorithm>
using namespace DirectX;

static float GEpsilon = std::numeric_limits<float>::epsilon();
static float GWorldMax = static_cast<float>(1 << 20);

namespace MathHelpers
{
    bool IsValidVector(XMFLOAT3 InVector);

    bool IsVectorNearlyZero(XMFLOAT3 InVector);

    bool IsVectorNearlyZero(XMFLOAT4 InVector);

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

    XMFLOAT3 LerpVector(const XMFLOAT3& InVector, const XMFLOAT3& InVector2, const float& T);
}

// operator for XMFLOAT3 multipication
XMFLOAT3 operator*(const XMFLOAT3& InVector, float InFloat);

// operator for XMFLOAT3 addition
XMFLOAT3 operator+(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);

// operator for XMFLOAT3 subtraction
XMFLOAT3 operator-(const XMFLOAT3& InVector, const XMFLOAT3& InVector2);