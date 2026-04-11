#include "Types.h"
#include "Runtime/CoreGlobals.h"
#include <algorithm>

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
    return std::sqrt((R * R + G * G + B * B));
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