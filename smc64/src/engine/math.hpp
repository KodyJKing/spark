#pragma once

#include "math/Vectors.hpp"

namespace Engine {
    // Commonly used for relative bone transforms.
    struct QuatTransform {
        Quaternion rotation;
        Vec3 translation;
        float scale;
    };

    // Commonly used for world space bone transforms.
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
    };

    Engine::Transform inverseTransform(Engine::Transform &wt);
    Engine::Transform multiplyTransforms(Engine::Transform & a, Engine::Transform & b);
    void orthonormalize(Engine::Transform &wt);
}
