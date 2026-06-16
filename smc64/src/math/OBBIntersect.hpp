#pragma once

#include "math/Vectors.hpp"
#include <cmath>

// Oriented bounding box math utilities: Euler→axes conversion and ray-OBB intersection.
// These are placed here (under /math) to keep the level-editor module focused on UI/logic.

namespace Math {

    // Ray is defined in Vectors.hpp as a global struct.
    using Ray = ::Ray;

    // Returns the closest point on an infinite axis to a given ray.
    // If the axis and ray are nearly parallel (degenerate), returns axisOrigin.
    // Use this to implement axis-drag gizmos: call once per frame with the mouse ray,
    // take axisDir · (P_new − P_prev) as the signed translation delta.
    inline Vec3 closestPointOnAxisToRay(const Ray& r, Vec3 axisOrigin, Vec3 axisDir) {
        Vec3 w    = axisOrigin - r.origin;
        float b   = axisDir.dot(r.direction);
        Vec3 rd   = r.direction; // non-const copy — Vec3::dot is not const-qualified
        float den = 1.f - b * b;                   // both dirs normalized
        if (fabsf(den) < 1e-6f) return axisOrigin;  // parallel fallback
        float t   = (axisDir.dot(w) - b * rd.dot(w)) / den;
        return axisOrigin + axisDir * t;
    }

    // Ray-OBB intersection using the separating axis / slab method.
    // axes[0..2] must be unit vectors (the OBB's local frame).
    // Returns true on hit; tOut is the distance along ray.direction to the entry point
    // (use the exit point if the ray origin is inside the OBB, in which case tOut < 0).
    inline bool rayIntersectsOBB(
        const Ray& ray,
        const Vec3& center,
        const Matrix3& axes,
        const Vec3& halfExtents,
        float& tOut)
    {
        constexpr float EPS = 1e-6f;
        Vec3 d = { center.x - ray.origin.x,
                   center.y - ray.origin.y,
                   center.z - ray.origin.z };

        float tMin = -1e30f, tMax = 1e30f;
        const float he[3] = { halfExtents.x, halfExtents.y, halfExtents.z };
        Vec3 axArr[3] = { axes.columns.x, axes.columns.y, axes.columns.z };

        for (int i = 0; i < 3; i++) {
            Vec3 axis = axArr[i]; // non-const copy — Vec3::dot is not const-qualified
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
