#pragma once

#include <cstddef>
#include "math/Vectors.hpp"

namespace Engine {
    // Commonly used for relative bone transforms.
    struct QuatTransform {
        Quaternion rotation;
        Vec3 translation;
        float scale;
    };

    // Commonly used for world space bone transforms.
    // Matches Ghidra's `Transform` struct exactly: w=s (scale), x/y/z=m.x/m.y/m.z
    // (orientation matrix, basis columns), pos=t (translation). Confirmed by
    // calling the real rotateVec/transformVec4AsPlane via smc64-dlltest --
    // see smc64-dlltest/src/tests/TransformTests.cpp.
    struct Transform {
        float w; // (scale)
        Vec3 x, y, z, pos; // (forward, left, up, translation)

        // Rotate a direction vector by this transform (no translation).
        Vec3 transformVec(Vec3 v) {
            return x * v.x + y * v.y + z * v.z;
        }

        // Transform a point by this transform (rotation + translation).
        Vec3 transformPoint(Vec3 p) {
            return x * p.x + y * p.y + z * p.z + pos;
        }

        static constexpr Transform identity() {
            return Transform{
                1.0f, 
                Vec3{1.0f, 0.0f, 0.0f}, 
                Vec3{0.0f, 1.0f, 0.0f}, 
                Vec3{0.0f, 0.0f, 1.0f}, 
                Vec3{0.0f, 0.0f, 0.0f}
            };
        }

        static Transform orthoNormal(Vec3 x, Vec3 z, Vec3 pos) {
            return Transform{
                1.0f,
                x,
                z.cross(x),
                z,
                pos
            };
        }
    };

    static_assert(sizeof(Transform) == 0x34);
    static_assert(offsetof(Transform, x) == 0x04);
    static_assert(offsetof(Transform, y) == 0x10);
    static_assert(offsetof(Transform, z) == 0x1c);
    static_assert(offsetof(Transform, pos) == 0x28);

    Engine::Transform inverseTransform(Engine::Transform &wt);
    Engine::Transform multiplyTransforms(Engine::Transform & a, Engine::Transform & b);
    void orthonormalize(Engine::Transform &wt);
}
