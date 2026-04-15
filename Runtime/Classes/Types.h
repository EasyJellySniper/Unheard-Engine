#pragma once
#include <cstdint>
#include <cmath>
#include <fstream>
#if _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

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

// point type
struct UHPoint
{
    UHPoint()
        : x(0)
        , y(0)
    {

    }

#if _WIN32
    UHPoint(POINT P)
        : x(P.x)
        , y(P.y)
    {

    }
#endif

    int32_t x;
    int32_t y;
};

// file I/O for UHPoint
extern std::ofstream& operator<<(std::ofstream& Out, const UHPoint& P);
extern std::ifstream& operator>>(std::ifstream& In, UHPoint& P);