#pragma once

#include <string>

struct Vec3 {
    float x, y, z;

    std::string toString();

    Vec3 operator+( const Vec3& other );
    Vec3 operator-( const Vec3& other );
    Vec3 operator*( float scalar );
    Vec3 operator/( float scalar );

    Vec3& operator+=( const Vec3& other );
    Vec3& operator-=( const Vec3& other );
    Vec3& operator*=( float scalar );
    Vec3& operator/=( float scalar );

    Vec3 cross( const Vec3& other );
    float dot( const Vec3& other );

    float length();
    float lengthSquared();

    // Returns a normalized *copy* of this vector.
    Vec3 normalize();

    // Keeps the component of this vector that is perpendicular to the given axis.
    Vec3 rejection(Vec3 axis);

    static Vec3 lerp( Vec3& a, Vec3& b, float t );
    Vec3 lerp( Vec3& other, float t );
    Vec3 projectToCone(Vec3 coneDirection, float coneAngle);

    static Vec3 randomGaussian(float stddev);
    static Vec3 randomUnitVector();

    // Given the X and Z column vectors of a rotation matrix (i.e. local +X and +Z axes),
    // returns the YXZ intrinsic Euler angles in degrees that reproduce that orientation.
    // Inverse of Matrix3::fromEulerYXZ().
    static Vec3 orientationToEuler_YXZ(Vec3 xCol, Vec3 zCol);

    // Gram-Schmidt: returns a new zCol that is orthogonal to xCol and normalized.
    // If zCol is nearly parallel to xCol (|dot| > 0.99), falls back to world +Y.
    static Vec3 orthonormalize(Vec3 xCol, Vec3 zCol);

    static Vec3 getTangent(Vec3 normal, Vec3 reference) {
        Vec3 tangent = normal.cross(reference);
        if (tangent.lengthSquared() < 1e-6f)
            tangent = normal.cross(Vec3{ 0.f, 1.f, 0.f });
        if (tangent.lengthSquared() < 1e-6f)
            tangent = normal.cross(Vec3{ 1.f, 0.f, 0.f });
        return tangent.normalize();
    }
};

struct Vec3i {
    int32_t x, y, z;

    Vec3i operator+( const Vec3i& other );
    Vec3i operator-( const Vec3i& other );
    Vec3i operator*( int32_t scalar );
    Vec3i operator/( int32_t scalar );

    Vec3i& operator+=( const Vec3i& other );
    Vec3i& operator-=( const Vec3i& other );
    Vec3i& operator*=( int32_t scalar );
    Vec3i& operator/=( int32_t scalar );

    float length();

    Vec3 toVec3();
};

struct Vec4 {
    float x, y, z, w;

    Vec4 operator+( const Vec4& other );
    Vec4 operator-( const Vec4& other );
    Vec4 operator*( float scalar );
    Vec4 operator/( float scalar );

    Vec4& operator+=( const Vec4& other );
    Vec4& operator-=( const Vec4& other );
    Vec4& operator*=( float scalar );
    Vec4& operator/=( float scalar );

    float dot( const Vec4& other );
};

struct Matrix3 {
    union {
        float m[9]; // Column-major order
        struct {
            Vec3 x, y, z; 
        } columns;
    };

    // Convert YXZ intrinsic Euler angles (degrees) to a rotation matrix.
    // Column layout: columns.x = local +X, columns.y = local +Y, columns.z = local +Z.
    // Rotation order: Ry(yaw) * Rx(pitch) * Rz(roll).
    static Matrix3 fromEulerYXZ(Vec3 eulerDeg);
};

struct Matrix4 {
    union {
        float m[16]; // Column-major order
        struct {
            Vec4 x, y, z, w; 
        } columns;
    };

    Matrix4 operator*( const Matrix4& other );
    Matrix4 operator*( float scalar );
    Matrix4 operator/( float scalar );
    Matrix4& operator*=( const Matrix4& other );
    Matrix4& operator*=( float scalar );
    Matrix4& operator/=( float scalar );

    Vec3 transform( Vec3 v, float w = 0.0f );
    Vec4 transform( Vec4 v );

    static Matrix4 identity();
    static Matrix4 translation( Vec3 translation );
    static Matrix4 scale( Vec3 scale );

    Matrix4 inverse();
};

struct Quaternion {
    float x, y, z, w;

    Quaternion operator+( Quaternion other );
    Quaternion operator-( Quaternion other );
    Quaternion operator*( Quaternion other );
    Quaternion operator/( Quaternion other );
    Quaternion operator*( float scalar );
    Quaternion operator/( float scalar );
    Quaternion& operator+=( Quaternion other );
    Quaternion& operator-=( Quaternion other );
    Quaternion& operator*=( Quaternion other );
    Quaternion& operator/=( Quaternion other );
    Quaternion& operator*=( float scalar );
    Quaternion& operator/=( float scalar );

    float lengthSquared();
    float length();
    float dot( Quaternion other );
    Quaternion conjugate();
    Quaternion normalize();
    Quaternion nlerp( Quaternion& other, float t, bool shortestPath = true );
    Quaternion pow( float exponent );
};

struct Ray {
    Vec3 origin;
    Vec3 direction; // should be normalized
};

// Returns the closest point on an infinite axis (axisOrigin + t·axisDir) to a ray.
// Returns axisOrigin when the axis and ray are nearly parallel (degenerate).
// Typical gizmo usage: call each frame with the mouse ray, project the delta
// onto axisDir to get the signed translation amount.
inline Vec3 closestPointOnAxisToRay(const Ray& r, Vec3 axisOrigin, Vec3 axisDir) {
    Vec3  w   = axisOrigin - r.origin;
    float b   = axisDir.dot(r.direction);
    Vec3  rd  = r.direction; // non-const copy — Vec3::dot is not const-qualified
    float den = 1.f - b * b;
    if (fabsf(den) < 1e-6f) return axisOrigin;
    float t   = (b * rd.dot(w) - axisDir.dot(w)) / den;
    return axisOrigin + axisDir * t;
}

struct Camera {
    Vec3 pos, fwd, up;
    float fov, width, height;
    bool verticalFov;
    Vec3 left();
    Vec3 project(Vec3 p);
    // Unproject a screen-space pixel (sx, sy) to a world-space ray from the camera.
    // Exact inverse of project() at unit depth. direction is normalized.
    Ray  mouseRay(float sx, float sy);
};

Vec3 orientationToEulerAngles(Vec3 fwd, Vec3 up);
