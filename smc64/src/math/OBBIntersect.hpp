#pragma once

#include "math/Vectors.hpp"
#include <array>
#include <cmath>

// Oriented bounding box math utilities: Euler→axes conversion and ray-OBB intersection.
// These are placed here (under /math) to keep the level-editor module focused on UI/logic.

namespace Math {

    struct Ray {
        Vec3 origin;
        Vec3 direction; // should be normalized
    };

    // Convert YXZ intrinsic Euler angles (degrees) to three orthogonal local axes.
    //   axes[0] = local +X (right)
    //   axes[1] = local +Y (up)
    //   axes[2] = local +Z (forward)
    // Rotation order: Ry(yaw) * Rx(pitch) * Rz(roll)  (intrinsic YXZ)
    inline std::array<Vec3, 3> eulerDegreesToAxes(Vec3 eulerDeg) {
        constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
        float cy = cosf(eulerDeg.y * DEG2RAD), sy = sinf(eulerDeg.y * DEG2RAD); // yaw
        float cx = cosf(eulerDeg.x * DEG2RAD), sx = sinf(eulerDeg.x * DEG2RAD); // pitch
        float cz = cosf(eulerDeg.z * DEG2RAD), sz = sinf(eulerDeg.z * DEG2RAD); // roll

        // Columns of R = Ry * Rx * Rz, derived analytically.
        Vec3 axisX = { cy*cz + sy*sx*sz,   cx*sz,  -sy*cz + cy*sx*sz };
        Vec3 axisY = { -cy*sz + sy*sx*cz,  cx*cz,   sy*sz + cy*sx*cz };
        Vec3 axisZ = { sy*cx,             -sx,       cy*cx             };
        return { axisX, axisY, axisZ };
    }

    // Ray-OBB intersection using the separating axis / slab method.
    // axes[0..2] must be unit vectors (the OBB's local frame).
    // Returns true on hit; tOut is the distance along ray.direction to the entry point
    // (use the exit point if the ray origin is inside the OBB, in which case tOut < 0).
    inline bool rayIntersectsOBB(
        const Ray& ray,
        const Vec3& center,
        const std::array<Vec3, 3>& axes,
        const Vec3& halfExtents,
        float& tOut)
    {
        constexpr float EPS = 1e-6f;
        Vec3 d = { center.x - ray.origin.x,
                   center.y - ray.origin.y,
                   center.z - ray.origin.z };

        float tMin = -1e30f, tMax = 1e30f;
        const float he[3] = { halfExtents.x, halfExtents.y, halfExtents.z };

        for (int i = 0; i < 3; i++) {
            Vec3 axis = axes[i]; // non-const copy — Vec3::dot is not const-qualified
            float e = axis.dot(d);
            float f = axis.dot(ray.direction);

            if (fabsf(f) > EPS) {
                float t1 = (e - he[i]) / f;
                float t2 = (e + he[i]) / f;
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) return false;
            } else {
                // Ray is parallel to this slab; check if origin is inside
                if (fabsf(e) > he[i]) return false;
            }
        }

        if (tMax < 0.0f) return false; // OBB is behind the ray
        tOut = (tMin >= 0.0f) ? tMin : tMax;
        return true;
    }

} // namespace Math
