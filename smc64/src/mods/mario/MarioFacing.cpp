#include "MarioFacing.hpp"

#include "libsm64.h"
#include "decomp/sm64.h"
#include "MarioState.hpp"
#include "engine/halo1.hpp"

#include <cmath>

namespace HaloCE::Mod::Mario {

    bool shouldAlignToLook() {
        if (marioInputs.stickX != 0 || marioInputs.stickY != 0) return false;

        auto playerController = Engine::getPlayerControllerPointer();
        bool triggerDown = playerController && playerController->gunTrigger > 0.5f;

        return marioState.action == ACT_IDLE || marioState.action == ACT_RIDING_SHELL_GROUND;
    }

    bool shouldWalkOnAlign() {
        return marioState.action == ACT_IDLE;
    }

    bool shouldAlignGradual() {
        return marioState.action != ACT_IDLE;
    }

    float facingTolerance() {
        const float PI = 3.14159265f;
        auto playerController = Engine::getPlayerControllerPointer();
        bool triggerDown = playerController && playerController->gunTrigger > 0.5f;
        if (triggerDown) {
            return PI / 6.0f;
        }
        if (marioState.action == ACT_RIDING_SHELL_GROUND) {
            return PI / 2.0f;
        }
        return PI * 2.0f;
    }

    void faceLookDirection(Vec3 lookDirection) {
        if (!shouldAlignToLook()) return;

        float desiredYaw = atan2f(lookDirection.x, lookDirection.y);

        float yaw = marioState.faceAngle;

        float dot = sinf(yaw) * sinf(desiredYaw) + cosf(yaw) * cosf(desiredYaw);
        float diff = acosf(dot);

        if (diff < facingTolerance()) return;

        if (shouldAlignGradual()) {
            float cross = cosf(yaw) * sinf(desiredYaw) - sinf(yaw) * cosf(desiredYaw);
            float sign = (cross > 0) ? 1.0f : -1.0f;
            float turnSpeed = 0.1f;
            desiredYaw = yaw + sign * turnSpeed * diff;
        }

        sm64_set_mario_faceangle(marioId, desiredYaw);
        if (shouldWalkOnAlign()) {
            sm64_set_mario_action(marioId, ACT_WALKING);
            sm64_set_mario_forward_velocity(marioId, 5.0f);
        }
    }

}
