#pragma once

#include "math/Vectors.hpp"

namespace Engine {
    // Used for relative bone transforms.
    struct Transform {
        Quaternion rotation;
        Vec3 translation;
        float scale;
    };

    // Used for world sppace bone transforms.
    struct WorldTransform {
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
    };

    Engine::WorldTransform inverseWorldTransform(Engine::WorldTransform &wt);
    Engine::WorldTransform multiplyWorldTransforms(Engine::WorldTransform & a, Engine::WorldTransform & b);
    void orthonormalize(Engine::WorldTransform &wt);
}
