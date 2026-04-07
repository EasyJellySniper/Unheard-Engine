#pragma once
#include <cstdint>

// math header used in UHE, this will contain custom types for vector and matrix
// or any math types that are not basic c++ types
const float G_PI = 3.141592653589793f;

// 2D vector
struct UHVector2
{
    UHVector2() = default;

    UHVector2(const UHVector2&) = default;
    UHVector2& operator=(const UHVector2&) = default;

    UHVector2(UHVector2&&) = default;
    UHVector2& operator=(UHVector2&&) = default;

    constexpr UHVector2(float _x, float _y) noexcept : X(_x), Y(_y) {}

    float X;
    float Y;
};

// 3D vector
struct UHVector3
{
    UHVector3() = default;

    UHVector3(const UHVector3&) = default;
    UHVector3& operator=(const UHVector3&) = default;

    UHVector3(UHVector3&&) = default;
    UHVector3& operator=(UHVector3&&) = default;

    constexpr UHVector3(float _x, float _y, float _z) noexcept : X(_x), Y(_y), Z(_z) {}

    float X;
    float Y;
    float Z;
};

// 4D vector
struct UHVector4
{
    UHVector4() = default;

    UHVector4(const UHVector4&) = default;
    UHVector4& operator=(const UHVector4&) = default;

    UHVector4(UHVector4&&) = default;
    UHVector4& operator=(UHVector4&&) = default;

    constexpr UHVector4(float _x, float _y, float _z, float _w) noexcept : X(_x), Y(_y), Z(_z), W(_w) {}

    float X;
    float Y;
    float Z;
    float W;
};

const UHVector4 GBoxOffset[8] =
{
    UHVector4{ -1.0f, -1.0f,  1.0f, 0.0f },
    UHVector4{  1.0f, -1.0f,  1.0f, 0.0f },
    UHVector4{  1.0f,  1.0f,  1.0f, 0.0f },
    UHVector4{ -1.0f,  1.0f,  1.0f, 0.0f },
    UHVector4{ -1.0f, -1.0f, -1.0f, 0.0f },
    UHVector4{  1.0f, -1.0f, -1.0f, 0.0f },
    UHVector4{  1.0f,  1.0f, -1.0f, 0.0f },
    UHVector4{ -1.0f,  1.0f, -1.0f, 0.0f }
};

// matrix 4x4
struct UHMatrix4x4
{
    UHMatrix4x4() = default;

    UHMatrix4x4(const UHMatrix4x4&) = default;
    UHMatrix4x4& operator=(const UHMatrix4x4&) = default;

    UHMatrix4x4(UHMatrix4x4&&) = default;
    UHMatrix4x4& operator=(UHMatrix4x4&&) = default;

    constexpr UHMatrix4x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) noexcept
        : _11(m00), _12(m01), _13(m02), _14(m03),
        _21(m10), _22(m11), _23(m12), _24(m13),
        _31(m20), _32(m21), _33(m22), _34(m23),
        _41(m30), _42(m31), _43(m32), _44(m33) {
    }

    static void CreateFromVector(UHMatrix4x4& OutM, const UHVector4& R0, const UHVector4& R1, const UHVector4& R2, const UHVector4& R3) noexcept;

    float  operator() (size_t Row, size_t Column) const noexcept { return M[Row][Column]; }
    float& operator() (size_t Row, size_t Column) noexcept { return M[Row][Column]; }

    union
    {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float M[4][4];
    };
};

// matrix 3x4
struct UHMatrix3x4
{
    UHMatrix3x4() = default;

    UHMatrix3x4(const UHMatrix3x4&) = default;
    UHMatrix3x4& operator=(const UHMatrix3x4&) = default;

    UHMatrix3x4(UHMatrix3x4&&) = default;
    UHMatrix3x4& operator=(UHMatrix3x4&&) = default;

    constexpr UHMatrix3x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23) noexcept
        : _11(m00), _12(m01), _13(m02), _14(m03),
        _21(m10), _22(m11), _23(m12), _24(m13),
        _31(m20), _32(m21), _33(m22), _34(m23) {
    }

    float  operator() (size_t Row, size_t Column) const noexcept { return M[Row][Column]; }
    float& operator() (size_t Row, size_t Column) noexcept { return M[Row][Column]; }

    union
    {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
        };
        float M[3][4];
    };
};

// bounding box
struct UHBoundingBox
{
    static constexpr size_t CORNER_COUNT = 8;

    // Creators
    UHBoundingBox() noexcept : Center(0, 0, 0), Extents(1.f, 1.f, 1.f) {}

    UHBoundingBox(const UHBoundingBox&) = default;
    UHBoundingBox& operator=(const UHBoundingBox&) = default;

    UHBoundingBox(UHBoundingBox&&) = default;
    UHBoundingBox& operator=(UHBoundingBox&&) = default;

    constexpr UHBoundingBox(const UHVector3& center, const UHVector3& extents) noexcept
        : Center(center), Extents(extents) {
    }

    // functions
    bool Contains(UHVector3 Point) const noexcept;
    bool ContainedBy(UHVector4 Plane0, UHVector4 Plane1, UHVector4 Plane2,
        UHVector4 Plane3, UHVector4 Plane4, UHVector4 Plane5) const noexcept;
    void Transform(UHBoundingBox& Out, UHMatrix4x4 M) const noexcept;
    void GetCorners(UHVector3* Corners) const noexcept;
    static void CreateMerged(UHBoundingBox& Out, const UHBoundingBox& b1, const UHBoundingBox& b2) noexcept;

    UHVector3 Center;            // Center of the box.
    UHVector3 Extents;           // Distance from the center to each side.
};

// bounding frustum
struct UHBoundingFrustum
{
    // Creators
    UHBoundingFrustum() noexcept :
        Origin(0, 0, 0), Orientation(0, 0, 0, 1.f), RightSlope(1.f), LeftSlope(-1.f),
        TopSlope(1.f), BottomSlope(-1.f), Near(0), Far(1.f) {
    }

    UHBoundingFrustum(const UHBoundingFrustum&) = default;
    UHBoundingFrustum& operator=(const UHBoundingFrustum&) = default;

    UHBoundingFrustum(UHBoundingFrustum&&) = default;
    UHBoundingFrustum& operator=(UHBoundingFrustum&&) = default;

    constexpr UHBoundingFrustum(const UHVector3& origin, const UHVector4& orientation,
        float rightSlope, float leftSlope, float topSlope, float bottomSlope,
        float nearPlane, float farPlane) noexcept
        : Origin(origin), Orientation(orientation),
        RightSlope(rightSlope), LeftSlope(leftSlope), TopSlope(topSlope), BottomSlope(bottomSlope),
        Near(nearPlane), Far(farPlane) {
    }

    // functions
    void Transform(UHBoundingFrustum& Out, UHMatrix4x4 M) const noexcept;
    bool Contains(const UHBoundingBox& box) const noexcept;
    static void CreateFromMatrix(UHBoundingFrustum& Out, UHMatrix4x4 Projection, bool rhcoords = false) noexcept;

    static constexpr size_t CORNER_COUNT = 8;

    UHVector3 Origin;            // Origin of the frustum (and projection).
    UHVector4 Orientation;       // Quaternion representing rotation.

    float RightSlope;           // Positive X (X/Z)
    float LeftSlope;            // Negative X
    float TopSlope;             // Positive Y (Y/Z)
    float BottomSlope;          // Negative Y
    float Near, Far;            // Z of the near plane and far plane.
};

// UHE math functions
namespace UHMathHelpers
{
    bool IsValidVector(UHVector3 InVector);

    bool IsVectorNearlyZero(UHVector3 InVector);

    bool IsVectorNearlyZero(UHVector4 InVector);

    bool IsVectorEqual(UHVector2 InA, UHVector2 InB);

    bool IsVectorEqual(UHVector3 InA, UHVector3 InB);

    bool IsVectorEqual(UHVector4 InA, UHVector4 InB);

    UHMatrix4x4 Identity4x4();

    UHMatrix3x4 MatrixTo3x4(UHMatrix4x4 InMatrix);

    UHMatrix3x4 Identity3x4();

    // Returns random float in [0, 1).
    float RandFloat();

    // matrix to pitch yaw roll, use for display only!
    void MatrixToPitchYawRoll(UHMatrix4x4 InMat, float& Pitch, float& Yaw, float& Roll);

    UHVector3 MinVector(const UHVector3& InVector, const UHVector3& InVector2);

    UHVector3 MaxVector(const UHVector3& InVector, const UHVector3& InVector2);

    float Halton(int32_t Index, int32_t Base);

    float Lerp(const float& InVal1, const float& InVal2, const float& T);
    UHVector3 LerpVector(const UHVector3& InVector, const UHVector3& InVector2, const float& T);

    float VectorDistanceSqr(const UHVector3& InVector, const UHVector3& InVector2);

    float RoundUpToClosestPowerOfTwo(float InVal);

    template <typename T>
    T RoundUpDivide(const T& InVal, const T& Divisor)
    {
        if (Divisor == 0)
        {
            return InVal;
        }

        return (InVal + Divisor) / Divisor;
    }

    float RoundUpToMultiple(float InVal, float InMultiple);

    // matrix functions
    UHMatrix4x4 UHMatrixTranspose(UHMatrix4x4 M);
    UHMatrix4x4 UHMatrixTranslation(UHVector3 V);
    UHMatrix4x4 UHMatrixScaling(UHVector3 V);
    UHMatrix4x4 UHMatrixPerspectiveFovLH(float FovAngleY, float Width, float Height, float NearZ, float FarZ);
    UHMatrix4x4 UHMatrixPerspectiveFovRH(float FovAngleY, float Width, float Height, float NearZ, float FarZ);
    UHMatrix4x4 UHMatrixLookToRH(UHVector3 Position, UHVector3 Forward, UHVector3 Up);
    UHMatrix4x4 UHMatrixInverse(UHMatrix4x4 M);
    UHMatrix4x4 UHMatrixRotationRollPitchYaw(float Pitch, float Yaw, float Roll) noexcept;
    UHMatrix4x4 UHMatrixRotationAxis(UHVector3 Axis, float Angle);

    // vector functions
    UHVector4 UHVector3Transform(UHVector3 V, UHMatrix4x4 M);
    UHVector3 UHVector3TransformNormal(UHVector3 V, UHMatrix4x4 M);
    bool UHVector3InBound(UHVector3 V, UHVector3 E);
    UHVector3 UHVector3Round(UHVector3 V);
    UHVector4 UHVector3Normalize(UHVector4 V);
    UHVector3 UHVector3Normalize(UHVector3 V);
    UHVector4 UHQuaternionRotationMatrix(UHMatrix4x4 M);
    UHVector4 UHQuaternionMultiply(UHVector4 Q1, UHVector4 Q2);
    float UHVector3Dot(UHVector4 V1, UHVector4 V2);
    float UHVector3Dot(UHVector3 V1, UHVector3 V2);
    UHVector4 UHVector4Transform(UHVector4 V, UHMatrix4x4 M);
    UHVector4 UHVector4Multiply(UHVector4 V1, UHVector4 V2);
    UHVector4 UHVector4SplatZ(UHVector4 V);
    UHVector4 UHVector4SplatW(UHVector4 V);
    UHVector4 UHVector4Reciprocal(UHVector4 V);
    UHVector3 UHVector3Rotate(UHVector4 V, UHVector4 Q);
    float UHVector4Dot(UHVector4 V1, UHVector4 V2);
    UHVector3 UHVector3Abs(UHVector3 V);

    // plane functions
    UHVector4 UHPlaneTransform(UHVector4 Plane, UHVector4 Rotation, UHVector3 Translation);
    UHVector4 UHPlaneNormalize(UHVector4 Plane);

    // intersection
    void FastIntersectAxisAlignedBoxPlane(UHVector4 Center, UHVector3 Extents, UHVector4 Plane,
        bool& Outside, bool& Inside) noexcept;

    // degrees
    float ToRadians(float InDegrees);
    float ToDegrees(float InRadians);
}

// UHVector3 operators
UHVector3 operator*(const UHVector3& InVector, float InFloat);
UHVector3 operator+(const UHVector3& InVector, const UHVector3& InVector2);
UHVector3 operator-(const UHVector3& InVector, const UHVector3& InVector2);
bool operator==(const UHVector3& InVector, const UHVector3& InVector2);

// UHVector4 operators
UHVector4 operator/=(const UHVector4& InVector, float InFloat);

// UHMatrix4x4 operators
UHMatrix4x4 operator*(const UHMatrix4x4& InA, const UHMatrix4x4& InB);