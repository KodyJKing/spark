#pragma once

#include <vector>
#include <string>
#include "MarioSkeleton.hpp"
#include "engine/halo1.hpp"

namespace Mod::Mario::InverseKinematics {
    struct IKBone {
        Engine::Transform initialTransform;
        float length = -1.0f;
        Vec3 pivotOffset = Vec3{ 0.0f, 0.0f, 0.0f };
        Engine::Transform currentTransform;
    };

    struct IKRequest {
        std::vector<IKBone> bones;
        Engine::Transform targetTransform;
        bool enforceRotation = false;
    };

    void solveLimbIK(IKRequest& request);

    struct MarioIKRequest {
        enum class Limb {
            LeftArm,
            RightArm,
        } limb;
        Engine::Transform targetTransform;
        bool enforceRotation = false;
    };

    void applyMarioIK(MarioIKRequest& request);
}
